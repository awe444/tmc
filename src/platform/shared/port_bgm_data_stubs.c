/**
 * @file port_bgm_data_stubs.c
 * @brief Strong host stand-ins for asm-only `SongHeader` blobs (`data/sound/sounds.s`).
 */
#ifdef __PORT__

/* Match legacy unresolved stub layout (char[4096], 16-byte aligned). */
#define PORT_STRONG_BGM_DATA(name) char name[4096] __attribute__((aligned(16)))

PORT_STRONG_BGM_DATA(bgmBeanstalk);
PORT_STRONG_BGM_DATA(bgmBeatVaati);
PORT_STRONG_BGM_DATA(bgmBossTheme);
PORT_STRONG_BGM_DATA(bgmCastleCollapse);
PORT_STRONG_BGM_DATA(bgmCastleMotif);
PORT_STRONG_BGM_DATA(bgmCastleTournament);
PORT_STRONG_BGM_DATA(bgmCastorWilds);
PORT_STRONG_BGM_DATA(bgmCaveOfFlames);
PORT_STRONG_BGM_DATA(bgmCloudTops);
PORT_STRONG_BGM_DATA(bgmCredits);
PORT_STRONG_BGM_DATA(bgmCrenelStorm);
PORT_STRONG_BGM_DATA(bgmCuccoMinigame);
PORT_STRONG_BGM_DATA(bgmDarkHyruleCastle);
PORT_STRONG_BGM_DATA(bgmDeepwoodShrine);
PORT_STRONG_BGM_DATA(bgmDiggingCave);
PORT_STRONG_BGM_DATA(bgmDungeon);
PORT_STRONG_BGM_DATA(bgmElementGet);
PORT_STRONG_BGM_DATA(bgmElementTheme);
PORT_STRONG_BGM_DATA(bgmElementalSanctuary);
PORT_STRONG_BGM_DATA(bgmEzloGet);
PORT_STRONG_BGM_DATA(bgmEzloStory);
PORT_STRONG_BGM_DATA(bgmEzloTheme);
PORT_STRONG_BGM_DATA(bgmFairyFountain);
PORT_STRONG_BGM_DATA(bgmFairyFountain2);
PORT_STRONG_BGM_DATA(bgmFestivalApproach);
PORT_STRONG_BGM_DATA(bgmFightTheme);
PORT_STRONG_BGM_DATA(bgmFightTheme2);
PORT_STRONG_BGM_DATA(bgmFileSelect);
PORT_STRONG_BGM_DATA(bgmFortressOfWinds);
PORT_STRONG_BGM_DATA(bgmGameover);
PORT_STRONG_BGM_DATA(bgmHouse);
PORT_STRONG_BGM_DATA(bgmHyruleCastle);
PORT_STRONG_BGM_DATA(bgmHyruleCastleNointro);
PORT_STRONG_BGM_DATA(bgmHyruleField);
PORT_STRONG_BGM_DATA(bgmHyruleTown);
PORT_STRONG_BGM_DATA(bgmIntroCutscene);
PORT_STRONG_BGM_DATA(bgmLearnScroll);
PORT_STRONG_BGM_DATA(bgmLostWoods);
PORT_STRONG_BGM_DATA(bgmLttpTitle);
PORT_STRONG_BGM_DATA(bgmMinishCap);
PORT_STRONG_BGM_DATA(bgmMinishVillage);
PORT_STRONG_BGM_DATA(bgmMinishWoods);
PORT_STRONG_BGM_DATA(bgmMtCrenel);
PORT_STRONG_BGM_DATA(bgmPalaceOfWinds);
PORT_STRONG_BGM_DATA(bgmPicoriFestival);
PORT_STRONG_BGM_DATA(bgmRoyalCrypt);
PORT_STRONG_BGM_DATA(bgmRoyalValley);
PORT_STRONG_BGM_DATA(bgmSavingZelda);
PORT_STRONG_BGM_DATA(bgmSecretCastleEntrance);
PORT_STRONG_BGM_DATA(bgmStory);
PORT_STRONG_BGM_DATA(bgmSwiftbladeDojo);
PORT_STRONG_BGM_DATA(bgmSyrupTheme);
PORT_STRONG_BGM_DATA(bgmTempleOfDroplets);
PORT_STRONG_BGM_DATA(bgmTitleScreen);
PORT_STRONG_BGM_DATA(bgmUnused);
PORT_STRONG_BGM_DATA(bgmVaatiMotif);
PORT_STRONG_BGM_DATA(bgmVaatiReborn);
PORT_STRONG_BGM_DATA(bgmVaatiTheme);
PORT_STRONG_BGM_DATA(bgmVaatiTransfigured);
PORT_STRONG_BGM_DATA(bgmVaatiWrath);
PORT_STRONG_BGM_DATA(bgmWindRuins);

#endif /* __PORT__ */
