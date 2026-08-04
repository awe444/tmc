package org.tmc.port;

import android.content.pm.PackageManager;
import android.content.res.AssetManager;
import android.os.Bundle;
import android.util.Log;

import org.json.JSONException;
import org.json.JSONObject;
import org.libsdl.app.SDLActivity;

import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;

/**
 * SDLActivity plus the one thing the port needs that a desktop install gets
 * for free: its data on a real filesystem.
 *
 * The engine reaches the filesystem through ordinary relative-path fopen()
 * throughout, and APK assets are not files. So the ROM and the pre-extracted
 * asset tree ride along in the APK under assets/gamedata/ and get mirrored
 * into the app's private files directory here. port_main.c chdir()s there
 * before its first open, which leaves every path in the engine working
 * exactly as it does on the desktop.
 *
 * Nothing here touches input. SDLActivity already routes game controllers
 * through SDL's gamepad API, and the native build compiles no touch handling
 * at all (PORT_NO_TOUCH_CONTROLS).
 */
public class TmcActivity extends SDLActivity {

    private static final String TAG = "tmc";

    /** Root inside the APK holding everything that is mirrored out. */
    private static final String STAGE_ROOT = "gamedata";

    /** Records which install the files directory was staged from. */
    private static final String STAMP = ".staged";

    /**
     * Timestamp stamped onto the staged ROM. Any fixed whole-second value
     * works; see repairAssetFingerprint() for why it has to be pinned to
     * something we know exactly.
     */
    private static final long ROM_MTIME_MS = 1262304000000L; // 2010-01-01T00:00:00Z

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        // Before super, which loads the native libraries and starts the thread
        // that calls into SDL_main.
        try {
            stageGameData();
        } catch (Exception e) {
            // Not fatal on its own: an incomplete tree makes the engine fall
            // back to extracting from the ROM on device, and a missing ROM
            // makes it show its own error. Both are better than dying here
            // with no message.
            Log.e(TAG, "staging game data failed", e);
        }
        super.onCreate(savedInstanceState);
    }

    private void stageGameData() throws IOException, PackageManager.NameNotFoundException {
        final File filesDir = getFilesDir();
        final File stamp = new File(filesDir, STAMP);

        // lastUpdateTime rather than versionCode: it changes on every install
        // of every build, so re-running `gradlew installDebug` restages
        // without anyone having to remember to bump a version.
        final long installedAt =
                getPackageManager().getPackageInfo(getPackageName(), 0).lastUpdateTime;
        final String want = Long.toString(installedAt);

        if (want.equals(readText(stamp))) {
            Log.i(TAG, "game data already staged for this install");
            return;
        }

        Log.i(TAG, "staging game data into " + filesDir);
        final long started = System.currentTimeMillis();
        copyAssetTree(getAssets(), STAGE_ROOT, filesDir);
        repairAssetFingerprint(filesDir);
        writeText(stamp, want);
        Log.i(TAG, "staged game data in " + (System.currentTimeMillis() - started) + " ms");
    }

    /**
     * Mirrors {@code assetPath} out of the APK into {@code destRoot}, dropping
     * the {@code gamedata/} prefix so the result matches a desktop install.
     */
    private static void copyAssetTree(AssetManager assets, String assetPath, File destRoot)
            throws IOException {
        final String[] children = assets.list(assetPath);
        if (children == null || children.length == 0) {
            copyAssetFile(assets, assetPath, destRoot);
            return;
        }
        for (String child : children) {
            copyAssetTree(assets, assetPath + "/" + child, destRoot);
        }
    }

    private static void copyAssetFile(AssetManager assets, String assetPath, File destRoot)
            throws IOException {
        // "gamedata/assets/gfx.pak" -> "<files>/assets/gfx.pak"
        final String relative = assetPath.substring(STAGE_ROOT.length() + 1);
        final File dest = new File(destRoot, relative);
        final File parent = dest.getParentFile();
        if (parent != null && !parent.isDirectory() && !parent.mkdirs()) {
            throw new IOException("could not create " + parent);
        }

        final byte[] buffer = new byte[1 << 16];
        try (InputStream in = assets.open(assetPath);
             OutputStream out = new FileOutputStream(dest)) {
            int read;
            while ((read = in.read(buffer)) != -1) {
                out.write(buffer, 0, read);
            }
        }
    }

    /**
     * Makes the staged asset tree look current to the engine.
     *
     * On launch the engine compares the ROM's size and modification time
     * against the values the extractor recorded, and re-extracts everything if
     * they disagree (AssetExtractorApi::RuntimeUpToDate). Neither value can
     * survive the trip as written: the ROM gets a fresh mtime when it is
     * copied out of the APK, and the recorded one is in the host toolchain's
     * filesystem-clock epoch, which is not the one this device reads back.
     *
     * So the ROM's mtime is pinned to a known value and the fingerprint is
     * rewritten here in the device's own terms. Getting this wrong is not
     * fatal -- the engine just re-extracts on first launch, taking a slow
     * boot and a progress bar to reach the same state.
     */
    private static void repairAssetFingerprint(File filesDir) throws IOException {
        final File rom = new File(filesDir, "baserom.gba");
        final File staged = new File(filesDir, "assets/asset_build_state.json");
        if (!rom.isFile() || !staged.isFile()) {
            Log.w(TAG, "no ROM or build state staged; the engine will re-extract on device");
            return;
        }

        if (!rom.setLastModified(ROM_MTIME_MS)) {
            Log.w(TAG, "could not set the ROM's mtime; the engine will re-extract on device");
            return;
        }
        // Read back rather than trusting the value: a filesystem that stores
        // whole seconds only would have truncated it.
        final long mtimeMs = rom.lastModified();

        try {
            final JSONObject state = new JSONObject(readText(staged));
            // std::filesystem::last_write_time on this platform counts
            // nanoseconds from the Unix epoch, which is what the engine
            // compares the recorded value against.
            state.put("rom_mtime", mtimeMs * 1000000L);
            state.put("rom_size", rom.length());
            writeText(new File(filesDir, "assets/.asset_build_state.json"), state.toString());
        } catch (JSONException e) {
            Log.w(TAG, "build state is not valid JSON; the engine will re-extract on device", e);
            return;
        }

        if (!staged.delete()) {
            Log.w(TAG, "could not remove the undotted build state copy");
        }
    }

    private static String readText(File file) throws IOException {
        if (!file.isFile()) {
            return null;
        }
        final byte[] bytes = new byte[(int) file.length()];
        try (InputStream in = new FileInputStream(file)) {
            int offset = 0;
            while (offset < bytes.length) {
                final int read = in.read(bytes, offset, bytes.length - offset);
                if (read < 0) {
                    break;
                }
                offset += read;
            }
        }
        return new String(bytes, "UTF-8");
    }

    private static void writeText(File file, String text) throws IOException {
        try (OutputStream out = new FileOutputStream(file)) {
            out.write(text.getBytes("UTF-8"));
        }
    }
}
