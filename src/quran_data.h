#pragma once
#include <map>
#include <string>
#include <vector>

namespace QuranData {
    // Font mappings for different languages
    inline const std::map<std::string, std::string> arabicFonts = {
        {"uthmanic", "fonts/UthmanicHafs_V22.ttf"},
    };
    
    inline const std::map<int, std::string> translationFontMappings = {
        {1, "fonts/American Captain.ttf"},      // English - Sahih International
        {2, "fonts/American Captain.ttf"},      // Oromo (Latin script)
        {3, "fonts/AbyssinicaSIL-Regular.ttf"}, // Amharic
        {4, "fonts/Jameel Noori Nastaleeq Kasheeda.ttf"} // Urdu
        // Add more language mappings as needed
    };

    inline const std::map<int, std::string> translationFontFamilies = {
        {1, "American Captain"},
        {2, "American Captain"},
        {3, "Abyssinica SIL"},
        {4, "Jameel Noori Nastaleeq"}
    };
    
    // Mapping translationId -> RTL flag
    inline const std::map<int, bool> translationDirectionIsRtl = {
        {1, false}, // English
        {2, false}, // Oromo
        {3, false}, // Amharic
        {4, true}   // Urdu
    };
    
    // Background video themes
    inline const std::map<std::string, std::string> backgroundThemes = {
        {"space", "videos/themes/stars.mp4"},
    };
    
    // Default assets
    inline const std::string defaultArabicFont = "fonts/UthmanicHafs_V22.ttf";
    inline const std::string defaultTranslationFont = "fonts/American Captain.ttf";
    inline const std::string defaultTranslationFontFamily = "American Captain";
    inline const std::string defaultBackgroundVideo = "videos/themes/stars.mp4";

    // Mapping reciterId -> full ayah JSON path (GAPPED mode)
    inline const std::map<int, std::string> reciterFiles = {
        {2,  "data/ayah-by-ayah/2_Abdur_Rahman_as_Sudais_Recitation/ayah-recitation-abdur-rahman-as-sudais-recitation.json"},
        {13, "data/ayah-by-ayah/13_Mahmoud_Khalil_Al_Husary/ayah-recitation-mahmoud-khalil-al-husary-murattal-hafs-957.json"},
        {14, "data/ayah-by-ayah/14_Hani_ar_Rifai_Recitation/ayah-recitation-hani-ar-rifai-recitation-murattal-hafs-68.json"},
        {15, "data/ayah-by-ayah/15_Saud_Al_Shuraim/ayah-recitation-saud-al-shuraim-murattal-hafs-960.json"},
        {16, "data/ayah-by-ayah/16_Yasser_Al_Dosari/ayah-recitation-yasser-al-dosari-murattal-hafs-961.json"},
        {17, "data/ayah-by-ayah/17_Muhammad_Siddiq_Al_Minshawi/ayah-recitation-muhammad-siddiq-al-minshawi-murattal-hafs-959.json"},
        {18, "data/ayah-by-ayah/18_Khalifa_Al_Tunaiji/ayah-recitation-khalifa-al-tunaiji-murattal-hafs-958.json"},
        {19, "data/ayah-by-ayah/19_Mishari_Rashid_al_Afasy/ayah-recitation-mishari-rashid-al-afasy-murattal-hafs-953.json"},
        {20, "data/ayah-by-ayah/20_Mahmoud_Khalil_Al_Husary/ayah-recitation-mahmoud-khalil-al-husary-murattal-hafs-955.json"},
        {21, "data/ayah-by-ayah/21_Abdul_Basit_Abdul_Samad/ayah-recitation-abdul-basit-abdul-samad-murattal-hafs-950.json"},
        {22, "data/ayah-by-ayah/22_Abdul_Rahman_Al_Sudais/ayah-recitation-abdul-rahman-al-sudais-murattal-hafs-951.json"},
        {23, "data/ayah-by-ayah/23_Mohamed_al_Tablawi_Recitation/ayah-recitation-mohamed-al-tablawi-recitation-murattal-hafs-73.json"},
        {24, "data/ayah-by-ayah/24_Mahmoud_Khalil_Al_Husary/ayah-recitation-mahmoud-khalil-al-husary-mujawwad-hafs-956.json"},
        {25, "data/ayah-by-ayah/25_Maher_Al_Muaiqly/ayah-recitation-maher-al-mu-aiqly-murattal-hafs-948.json"},
        {26, "data/ayah-by-ayah/26_Abdul_Basit_Abdul_Samad/ayah-recitation-abdul-basit-abdul-samad-mujawwad-hafs-949.json"},
        {27, "data/ayah-by-ayah/27_Abu_Bakr_al_Shatri/ayah-recitation-abu-bakr-al-shatri-murattal-hafs-952.json"},
        {28, "data/ayah-by-ayah/28_Saad_al_Ghamdi/ayah-recitation-saad-al-ghamdi-murattal-hafs-954.json"}
    };

    // Mapping reciterId -> surah-by-surah directory (GAPLESS mode)
    inline const std::map<int, std::string> gaplessReciterDirs = {
        {1,  "data/surah-by-surah/1_Ahmad_Alnufais_Recitation"},
        {3,  "data/surah-by-surah/3_Muhammad_Siddiq_al_Minshawiwith_kids"},
        {4,  "data/surah-by-surah/4_Hady_Toure"},
        {5,  "data/surah-by-surah/5_Mishari_Rashid_al_Afasy_Streaming"},
        {6,  "data/surah-by-surah/6_Abdul_Basit_Abdul_Samad"},
        {7,  "data/surah-by-surah/7_Abdullah_Ali_Jabir"},
        {8,  "data/surah-by-surah/8_Yasser_ad_Dussary"},
        {9,  "data/surah-by-surah/9_Abdul_Basit_Abdul_Samad"},
        {10, "data/surah-by-surah/10_Mishari_Rashid_al_Afasy"},
        {11, "data/surah-by-surah/11_Abdullah_Awad_al_Juhani"},
        {12, "data/surah-by-surah/12_Abdullah_Basfar"},
        {29, "data/surah-by-surah/29_Mohammad_Al_Tablawi"},
        {30, "data/surah-by-surah/30_Mahmood_Ali_Al_Bana"},
        {31, "data/surah-by-surah/31_Muhammad_Jibreel"},
        {32, "data/surah-by-surah/32_Mahmoud_Khaleel_Al_Husary"},
        {33, "data/surah-by-surah/33_Khalid_Al_Jalil"},
        {34, "data/surah-by-surah/34_Mahmoud_Khalil_Al_Husary_Muallim"},
        {35, "data/surah-by-surah/35_Aziz_Alili"},
        {36, "data/surah-by-surah/36_Nasser_Al_Qatami"},
        {37, "data/surah-by-surah/37_Bandar_Baleela"},
        {38, "data/surah-by-surah/38_Hani_ar_Rifai"},
        {39, "data/surah-by-surah/39_Salah_Bukhatir"},
        {40, "data/surah-by-surah/40_Salah_al_Budair"},
        {41, "data/surah-by-surah/41_Mishari_ibn_Rashid_al_Afasy_with_Ibrahim_Walk_Saheeh_Intl_Translation"},
        {42, "data/surah-by-surah/42_Akram_Al_Alaqmi"},
        {43, "data/surah-by-surah/43_Abu_Bakr_al_Shatri"},
        {44, "data/surah-by-surah/44_Saad_al_Ghamdi"},
        {45, "data/surah-by-surah/45_Ali_Hajjaj_Alsouasi"},
        {46, "data/surah-by-surah/46_Saud_ash_Shuraym"},
        {47, "data/surah-by-surah/47_Abdullah_Basfar_with_Ibrahim_Walk_Saheeh_Intl_Translation"},
        {48, "data/surah-by-surah/48_Khalifah_Taniji"},
        {49, "data/surah-by-surah/49_Mahmoud_Khaleel_Al_Husary"},
        {50, "data/surah-by-surah/50_Fares_Abbad"},
        {51, "data/surah-by-surah/51_Mostafa_Ismaeel"},
        {52, "data/surah-by-surah/52_Muhammad_Siddiq_al_Minshawi"},
        {53, "data/surah-by-surah/53_Maher_al_Muaiqly"},
        {54, "data/surah-by-surah/54_Muhammad_Siddiq_al_Minshawi"},
        {55, "data/surah-by-surah/55_Yasser_ad_Dussary"},
        {56, "data/surah-by-surah/56_Abdullah_Matroud"},
        {57, "data/surah-by-surah/57_Ahmad_Nauina"},
        {58, "data/surah-by-surah/58_Abdur_Rahman_as_Sudais"},
        {59, "data/surah-by-surah/59_Sahl_Yasin"}
    };

    // Here as fallback
    inline const std::map<int, std::string> reciterNames = {
        // Gapless mode reciters (1, 3-12, 29-59)
        {1,  "Ahmad Alnufais"},
        {3,  "Muhammad Siddiq al-Minshawi with Kids"},
        {4,  "Hady Toure"},
        {5,  "Mishari Rashid al-Afasy"},
        {6,  "Abdul Basit Abdul Samad"},
        {7,  "Abdullah Ali Jabir"},
        {8,  "Yasser ad-Dussary"},
        {9,  "Abdul Basit Abdul Samad"},
        {10, "Mishari Rashid al-Afasy"},
        {11, "Abdullah Awad al-Juhani"},
        {12, "Abdullah Basfar"},
        {29, "Mohammad Al-Tablawi"},
        {30, "Mahmood Ali Al-Bana"},
        {31, "Muhammad Jibreel"},
        {32, "Mahmoud Khaleel Al-Husary"},
        {33, "Khalid Al-Jalil"},
        {34, "Mahmoud Khalil Al-Husary Muallim"},
        {35, "Aziz Alili"},
        {36, "Nasser Al-Qatami"},
        {37, "Bandar Baleela"},
        {38, "Hani ar-Rifai"},
        {39, "Salah Bukhatir"},
        {40, "Salah al-Budair"},
        {41, "Mishari ibn Rashid al-Afasy"},
        {42, "Akram Al-Alaqmi"},
        {43, "Abu Bakr al-Shatri"},
        {44, "Saad al-Ghamdi"},
        {45, "Ali Hajjaj Alsouasi"},
        {46, "Saud ash-Shuraym"},
        {47, "Abdullah Basfar"},
        {48, "Khalifah Taniji"},
        {49, "Mahmoud Khaleel Al-Husary"},
        {50, "Fares Abbad"},
        {51, "Mostafa Ismaeel"},
        {52, "Muhammad Siddiq al-Minshawi"},
        {53, "Maher al-Muaiqly"},
        {54, "Muhammad Siddiq al-Minshawi"},
        {55, "Yasser ad-Dussary"},
        {56, "Abdullah Matroud"},
        {57, "Ahmad Nauina"},
        {58, "Abdur Rahman as-Sudais"},
        {59, "Sahl Yasin"},
        
        // Gapped mode reciters (2, 13-28)
        {2,  "Abdur Rahman as-Sudais"},
        {13, "Mahmoud Khalil Al-Husary"},
        {14, "Hani ar-Rifai"},
        {15, "Saud Al-Shuraim"},
        {16, "Yasser Al-Dosari"},
        {17, "Muhammad Siddiq Al-Minshawi"},
        {18, "Khalifa Al-Tunaiji"},
        {19, "Mishari Rashid al-Afasy"},
        {20, "Mahmoud Khalil Al-Husary"},
        {21, "Abdul Basit Abdul Samad"},
        {22, "Abdul Rahman Al-Sudais"},
        {23, "Mohamed al-Tablawi"},
        {24, "Mahmoud Khalil Al-Husary"},
        {25, "Maher Al-Muaiqly"},
        {26, "Abdul Basit Abdul Samad"},
        {27, "Abu Bakr al-Shatri"},
        {28, "Saad al-Ghamdi"}
    };

    // Mapping translationId -> translation JSON path
    inline const std::map<int, std::string> translationFiles = {
        {1, "data/translations/en/en-sahih-international-clean.json"},
        {2, "data/translations/om/ghali-apapur-apaghuna-clean.json"},
        {3, "data/translations/amh/am-sadiq-simple.json"},
        {4, "data/translations/urd/ur-fatah-muhammad-jalandhari-simple.json"}
    };
    
    // Language names for translation IDs
    inline const std::map<int, std::string> translationLanguages = {
        {1, "en"},
        {2, "om"},
        {3, "amh"},
        {4, "urd"}
    };

    // Here as fallback
    inline const std::map<int, std::string> surahNames = {
        {1, "Al-Fatiha"},
        {2, "Al-Baqarah"},
        {3, "Al-Imran"},
        {4, "An-Nisa"},
        {5, "Al-Ma'idah"},
        {6, "Al-An'am"},
        {7, "Al-A'raf"},
        {8, "Al-Anfal"},
        {9, "At-Tawbah"},
        {10, "Yunus"},
        {11, "Hud"},
        {12, "Yusuf"},
        {13, "Ar-Ra'd"},
        {14, "Ibrahim"},
        {15, "Al-Hijr"},
        {16, "An-Nahl"},
        {17, "Al-Isra"},
        {18, "Al-Kahf"},
        {19, "Maryam"},
        {20, "Taha"},
        {21, "Al-Anbiya"},
        {22, "Al-Hajj"},
        {23, "Al-Mu'minun"},
        {24, "An-Nur"},
        {25, "Al-Furqan"},
        {26, "Ash-Shu'ara"},
        {27, "An-Naml"},
        {28, "Al-Qasas"},
        {29, "Al-Ankabut"},
        {30, "Ar-Rum"},
        {31, "Luqman"},
        {32, "As-Sajda"},
        {33, "Al-Ahzab"},
        {34, "Saba"},
        {35, "Fatir"},
        {36, "Ya-Sin"},
        {37, "As-Saffat"},
        {38, "Sad"},
        {39, "Az-Zumar"},
        {40, "Ghafir"},
        {41, "Fussilat"},
        {42, "Ash-Shura"},
        {43, "Az-Zukhruf"},
        {44, "Ad-Dukhan"},
        {45, "Al-Jathiya"},
        {46, "Al-Ahqaf"},
        {47, "Muhammad"},
        {48, "Al-Fath"},
        {49, "Al-Hujurat"},
        {50, "Qaf"},
        {51, "Adh-Dhariyat"},
        {52, "At-Tur"},
        {53, "An-Najm"},
        {54, "Al-Qamar"},
        {55, "Ar-Rahman"},
        {56, "Al-Waqiah"},
        {57, "Al-Hadid"},
        {58, "Al-Mujadilah"},
        {59, "Al-Hashr"},
        {60, "Al-Mumtahina"},
        {61, "As-Saff"},
        {62, "Al-Jumuah"},
        {63, "Al-Munafiqun"},
        {64, "At-Taghabun"},
        {65, "At-Talaq"},
        {66, "At-Tahrim"},
        {67, "Al-Mulk"},
        {68, "Al-Qalam"},
        {69, "Al-Haqqa"},
        {70, "Al-Ma'arij"},
        {71, "Nuh"},
        {72, "Al-Jinn"},
        {73, "Al-Muzzammil"},
        {74, "Al-Muddathir"},
        {75, "Al-Qiyama"},
        {76, "Al-Insan"},
        {77, "Al-Mursalat"},
        {78, "An-Naba"},
        {79, "An-Nazi'at"},
        {80, "Abasa"},
        {81, "At-Takwir"},
        {82, "Al-Infitar"},
        {83, "Al-Mutaffifin"},
        {84, "Al-Inshiqaq"},
        {85, "Al-Buruj"},
        {86, "At-Tariq"},
        {87, "Al-Ala"},
        {88, "Al-Ghashiya"},
        {89, "Al-Fajr"},
        {90, "Al-Balad"},
        {91, "Ash-Shams"},
        {92, "Al-Layl"},
        {93, "Ad-Duha"},
        {94, "Ash-Sharh"},
        {95, "At-Tin"},
        {96, "Al-Alaq"},
        {97, "Al-Qadr"},
        {98, "Al-Bayyina"},
        {99, "Az-Zalzala"},
        {100, "Al-Adiyat"},
        {101, "Al-Qaria"},
        {102, "At-Takathur"},
        {103, "Al-Asr"},
        {104, "Al-Humaza"},
        {105, "Al-Fil"},
        {106, "Quraish"},
        {107, "Al-Ma'un"},
        {108, "Al-Kawthar"},
        {109, "Al-Kafirun"},
        {110, "An-Nasr"},
        {111, "Al-Masad"},
        {112, "Al-Ikhlas"},
        {113, "Al-Falaq"},
        {114, "An-Nas"}
    };
    
    // Helper function to get font for translation
    inline std::string getTranslationFont(int translationId) {
        auto it = translationFontMappings.find(translationId);
        if (it != translationFontMappings.end()) {
            return it->second;
        }
        return defaultTranslationFont;
    }

    inline std::string getTranslationFontFamily(int translationId) {
        auto it = translationFontFamilies.find(translationId);
        if (it != translationFontFamilies.end()) {
            return it->second;
        }
        return defaultTranslationFontFamily;
    }

    inline std::string getTranslationLanguageCode(int translationId) {
        auto it = translationLanguages.find(translationId);
        if (it != translationLanguages.end()) {
            return it->second;
        }
        return "en";
    }

    inline bool isTranslationRtl(int translationId) {
        auto it = translationDirectionIsRtl.find(translationId);
        if (it != translationDirectionIsRtl.end()) {
            return it->second;
        }
        return false;
    }
}
