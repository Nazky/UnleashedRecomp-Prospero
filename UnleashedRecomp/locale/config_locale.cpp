#include <user/config.h>

/*
    !!!!!!!!!!!!!!!!!!!!!!!!!!!!!! LOCALISATION NOTES !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!

    - Ensure brand names are always presented on the same line.

      Correct:
      This is a string that contains a brand name like
      Xbox 360, which is one of the few consoles to have a port of
      Sonic Unleashed.

      Incorrect:
      This is a string that contains a brand name like Xbox
      360, which is one of the few consoles to have a port of Sonic
      Unleashed.

    - Ensure your locale is added in the correct order following the language enum.

      Correct:
      {
          { ELanguage::English,  "Example" },
          { ELanguage::Japanese, "Example" },
          { ELanguage::German,   "Example" },
          { ELanguage::French,   "Example" },
          { ELanguage::Spanish,  "Example" },
          { ELanguage::Italian,  "Example" }
      }

      Incorrect:
      {
          { ELanguage::English,  "Example" },
          { ELanguage::French,   "Example" },
          { ELanguage::Spanish,  "Example" },
          { ELanguage::German,   "Example" },
          { ELanguage::Italian,  "Example" },
          { ELanguage::Japanese, "Example" }
      }

    - Japanese localization should use furigana for message windows, options items,
    options descriptions and installer text.

      Furigana should be included in the text as such:
      "[私:わたし]はジョージです。"

    !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
*/

#define CONFIG_DEFINE_LOCALE(name) \
    CONFIG_LOCALE g_##name##_locale =

#define CONFIG_DEFINE_ENUM_LOCALE(type) \
    CONFIG_ENUM_LOCALE(type) g_##type##_locale =

// Japanese Notes: This localization should include furigana.
CONFIG_DEFINE_LOCALE(Language)
{
    { ELanguage::English,  { "Language", "Change the language used for text and logos." } },
    { ELanguage::Japanese, { "[言語:げんご]", "ゲーム[内:ない]の\u200B[表示言語:ひょうじげんご]を\u200B[変更:へんこう]できます" } },
    { ELanguage::German,   { "Sprache", "Ändere die Sprache für Text und Logos." } },
    { ELanguage::French,   { "Langue", "Modifie la langue utilisée pour le texte et logos." } },
    { ELanguage::Spanish,  { "Idioma", "Cambia el idioma utilizado para el texto y los logos." } },
    { ELanguage::Italian,  { "Lingua", "Cambia la lingua utilizzata per i testi e i logo." } }
};

// Notes: do not localise this.
CONFIG_DEFINE_ENUM_LOCALE(ELanguage)
{
    {
        ELanguage::English,
        {
            { ELanguage::English,  { "ENGLISH", "" } },
            { ELanguage::Japanese, { "日本語", "" } },
            { ELanguage::German,   { "DEUTSCH", "" } },
            { ELanguage::French,   { "FRANÇAIS", "" } },
            { ELanguage::Spanish,  { "ESPAÑOL", "" } },
            { ELanguage::Italian,  { "ITALIANO", "" } }
        }
    }
};

CONFIG_DEFINE_ENUM_LOCALE(EVoiceLanguage)
{
    {
        ELanguage::English,
        {
            { EVoiceLanguage::English,  { "ENGLISH", "" } },
            { EVoiceLanguage::Japanese, { "JAPANESE", "" } }
        }
    },
    {
        ELanguage::Japanese,
        {
            { EVoiceLanguage::English,  { "英語", "" } },
            { EVoiceLanguage::Japanese, { "日本語", "" } }
        }
    },
    {
        ELanguage::German,
        {
            { EVoiceLanguage::English,  { "ENGLISCH", "" } },
            { EVoiceLanguage::Japanese, { "JAPANISCH", "" } }
        }
    },
    {
        ELanguage::French,
        {
            { EVoiceLanguage::English,  { "ANGLAIS", "" } },
            { EVoiceLanguage::Japanese, { "JAPONAIS", "" } }
        }
    },
    {
        ELanguage::Spanish,
        {
            { EVoiceLanguage::English,  { "INGLÉS", "" } },
            { EVoiceLanguage::Japanese, { "JAPONÉS", "" } }
        }
    },
    {
        ELanguage::Italian,
        {
            { EVoiceLanguage::English,  { "INGLESE", "" } },
            { EVoiceLanguage::Japanese, { "GIAPPONESE", "" } }
        }
    },
};

// Japanese Notes: This localization should include furigana.
CONFIG_DEFINE_LOCALE(Hints)
{
    { ELanguage::English,  { "Hints", "Show hints during gameplay." } },
    { ELanguage::Japanese, { "ヒントリング", "ゲーム[内:ない]に\u200Bヒントリングを\u200B[表示:ひょうじ]するか\u200B[選択:せんたく]できます" } },
    { ELanguage::German,   { "Hinweise", "Zeige Hinweise während des Spiels." } },
    { ELanguage::French,   { "Indices", "Affiche les indices pendant le jeu." } },
    { ELanguage::Spanish,  { "Pistas", "Muestra pistas durante el juego." } },
    { ELanguage::Italian,  { "Indizi", "Mostra degli indizzi durante il gioco." } }
};

// Japanese Notes: This localization should include furigana.
CONFIG_DEFINE_LOCALE(ControlTutorial)
{
    { ELanguage::English,  { "Control Tutorial", "Show controller hints during gameplay.\n\nThe Werehog Critical Attack prompt will be unaffected." } },
    { ELanguage::Japanese, { "アクションナビ", "ゲーム[内:ない]に\u200Bアクションナビを\u200B[表示:ひょうじ]するか\u200B[選択:せんたく]できます\n\n「チャンスアタック」の\u200B[表示:ひょうじ]は\u200B[影響:えいきょう]されません" } },
    { ELanguage::German,   { "Steuerungsanleitung", "Zeige Steuerungshinweise während des Spiels.\n\nKritische Angriffe des Werehogs werden hiervon nicht beeinflusst." } },
    { ELanguage::French,   { "Indication des commandes", "Affiche les indications des commandes pendant le jeu.\n\nCeci n'affecte pas les Coup critique du Werehog." } },
    { ELanguage::Spanish,  { "Tutorial de controles", "Muestra pistas de controles durante el juego.\n\nEl indicador de ataque crítico del Werehog no se verá afectado." } },
    { ELanguage::Italian,  { "Tutorial dei comandi", "Mostra i tutorial dei comandi durante il gioco.\n\nIl tutorial per l'attacco critico del Werehog non verrà influenzato da questa opzione." } }
};

// Japanese Notes: This localization should include furigana.
CONFIG_DEFINE_LOCALE(AchievementNotifications)
{
    { ELanguage::English,  { "Achievement Notifications", "Show notifications for unlocking achievements.\n\nAchievements will still be rewarded with notifications disabled." } },
    { ELanguage::Japanese, { "[実績通知:じっせきつうち]", "[実績通知:じっせきつうち]の\u200B[有無:うむ]を\u200B[選択:せんたく]できます\n\nオフにしても\u200B[実績:じっせき]は\u200B[付与:ふよ]されます" } },
    { ELanguage::German,   { "Erfolgsbenachrichtigungen", "Zeige Benachrichtigungen für das Freischalten von Erfolgen.\n\nErfolge werden weiterhin freigeschaltet, auch wenn die Benachrichtigungen ausgeschaltet sind." } },
    { ELanguage::French,   { "Notification des succès", "Affiche les notifications pour le déverrouillage des succès.\n\nLes succès seront toujours obtenus même si les notifications sont désactivées." } },
    { ELanguage::Spanish,  { "Notificaciones de logros", "Muestra notificaciones al desbloquear logros.\n\nLos logros se seguirán obteniendo aunque las notificaciones estén desactivadas." } },
    { ELanguage::Italian,  { "Notifiche obiettivi", "Mostra delle notifiche quando sblocchi degli obiettivi.\n\nGli obiettivi verranno comunque assegnati anche con le notifiche disattivate." } }
};

// Japanese Notes: This localization should include furigana.
CONFIG_DEFINE_LOCALE(TimeOfDayTransition)
{
    { ELanguage::English,  { "Time of Day Transition", "Change how the loading screen appears when switching time of day in the hub areas." } },
    { ELanguage::Japanese, { "[昼夜:ちゅうや]きりかえのシーン", "[昼夜:ちゅうや]きりかえの\u200Bシーンを\u200B[選択:せんたく]できます" } },
    { ELanguage::German,   { "Tageszeit Übergang", "Verändere den Ladebildschirm beim Verändern der Tageszeit in Städten und Eingangslevel." } },
    { ELanguage::French,   { "Transition jour/nuit", "Change l'écran de chargement lors de la transition dans le hub." } },
    { ELanguage::Spanish,  { "Transición de hora del día", "Cambia cómo aparece la pantalla de carga al cambiar la hora del día en las áreas principales." } },
    { ELanguage::Italian,  { "Transizione giorno/notte", "Modifica l'aspetto della schermata di caricamento quando fai scorrere il tempo." } }
};

// Japanese Notes: This localization should include furigana in its description.
CONFIG_DEFINE_ENUM_LOCALE(ETimeOfDayTransition)
{
    {
        ELanguage::English,
        {
            { ETimeOfDayTransition::Xbox,        { "XBOX", "Xbox: the transformation cutscene will play with artificial loading times." } },
            { ETimeOfDayTransition::PlayStation, { "PLAYSTATION", "PlayStation: a spinning medal loading screen will be used instead." } }
        }
    },
    {
        ELanguage::Japanese,
        {
            { ETimeOfDayTransition::Xbox,        { "XBOX", "Xbox: [変身:へんしん]シーンが\u200B[人工的:じんこうてき]な\u200B[読:よ]み[込:こ]み[時間:じかん]で\u200B[再生:さいせい]されます" } },
            { ETimeOfDayTransition::PlayStation, { "PLAYSTATION", "PlayStation: [回転:かいてん]するメダルの\u200B[読:よ]み[込:こ]み[画面:がめん]が\u200B[使用:しよう]されます" } }
        }
    },
    {
        ELanguage::German,
        {
            { ETimeOfDayTransition::Xbox,        { "XBOX", "Xbox: Eine Transformationszwischensequenz mit künstlicher Ladezeit wird abgespielt." } },
            { ETimeOfDayTransition::PlayStation, { "PLAYSTATION", "PlayStation: Eine sich drehende Medallie wird stattdessen als Ladebildschirm verwendet." } }
        }
    },
    {
        ELanguage::French,
        {
            { ETimeOfDayTransition::Xbox,        { "XBOX", "Xbox : la scène de transformation sera jouée avec des temps de chargement artificiels." } },
            { ETimeOfDayTransition::PlayStation, { "PLAYSTATION", "PlayStation : un écran de chargement avec une médaille tournoyante sera utilisé à la place." } }
        }
    },
    {
        ELanguage::Spanish,
        {
            { ETimeOfDayTransition::Xbox,        { "XBOX", "Xbox: la cinemática de transformación se reproducirá con tiempos de carga artificiales." } },
            { ETimeOfDayTransition::PlayStation, { "PLAYSTATION", "PlayStation: se usará una pantalla de carga con una medalla giratoria." } }
        }
    },
    {
        ELanguage::Italian,
        {
            { ETimeOfDayTransition::Xbox,        { "XBOX", "Xbox: la scena di trasformazione verrà riprodotta con tempi di caricamento artificiali." } },
            { ETimeOfDayTransition::PlayStation, { "PLAYSTATION", "PlayStation: verrà utilizzata una schermata di caricamento con una medaglia che gira." } }
        }
    }
};

// Japanese Notes: This localization should include furigana.
CONFIG_DEFINE_LOCALE(ControllerIcons)
{
    { ELanguage::English,  { "Controller Icons", "Change the icons to match your controller." } },
    { ELanguage::Japanese, { "コントローラーアイコン", "ゲーム[内:ない]の\u200Bコントローラーアイコン\u200Bを[変更:へんこう]できます" } },
    { ELanguage::German,   { "Controllersymbole", "Ändere die Controllersymbole, um sie auf dein Modell anzupassen." } },
    { ELanguage::French,   { "Icône des boutons", "Modifie les icônes pour les faire correspondre à votre manette." } },
    { ELanguage::Spanish,  { "Iconos del mando", "Cambia los iconos para que coincidan con tu mando." } },
    { ELanguage::Italian,  { "Icone dei tasti", "Modifica le icone per farle corrispondere con il tuo controller." } }
};

// Japanese Notes: This localization should include furigana in its description.
CONFIG_DEFINE_ENUM_LOCALE(EControllerIcons)
{
    {
        ELanguage::English,
        {
            { EControllerIcons::Auto,        { "AUTO", "Auto: the game will determine which icons to use based on the current input device." } },
            { EControllerIcons::Xbox,        { "XBOX", "" } },
            { EControllerIcons::PlayStation, { "PLAYSTATION", "" } }
        }
    },
    {
        ELanguage::Japanese,
        {
            { EControllerIcons::Auto,        { "自動検出", "[\u2005自動検出\u2005:じどうけんしゅつ]: コントローラーアイコン\u200Bを[使用:しよう]している\u200B[\u2005入\u2005力\u2005:にゅうりょく]デバイスに\u200B[応:おう]じて\u200B[自動的:じどうてき]に\u200B[決定:けってい]します" } },
            { EControllerIcons::Xbox,        { "XBOX", "" } },
            { EControllerIcons::PlayStation, { "PLAYSTATION", "" } }
        }
    },
    {
        ELanguage::German,
        {
            { EControllerIcons::Auto,        { "AUTO", "Auto: Das Spiel erkennt automatisch deinen Controller um die Symbole dementsprechend anzupassen." } },
            { EControllerIcons::Xbox,        { "XBOX", "" } },
            { EControllerIcons::PlayStation, { "PLAYSTATION", "" } }
        }
    },
    {
        ELanguage::French,
        {
            { EControllerIcons::Auto,        { "AUTO", "Auto : le jeu déterminera automatiquement quelles icônes utiliser en fonction du périphérique d'entrée." } },
            { EControllerIcons::Xbox,        { "XBOX", "" } },
            { EControllerIcons::PlayStation, { "PLAYSTATION", "" } }
        }
    },
    {
        ELanguage::Spanish,
        {
            { EControllerIcons::Auto,        { "AUTO", "Auto: el juego determinará de forma automática qué iconos utilizar dependiendo del dispositivo de entrada actual." } },
            { EControllerIcons::Xbox,        { "XBOX", "" } },
            { EControllerIcons::PlayStation, { "PLAYSTATION", "" } }
        }
    },
    {
        ELanguage::Italian,
        {
            { EControllerIcons::Auto,        { "AUTO", "Auto: il gioco determinerà quali icone da utilizzare in base al dispositivo di input attuale." } },
            { EControllerIcons::Xbox,        { "XBOX", "" } },
            { EControllerIcons::PlayStation, { "PLAYSTATION", "" } }
        }
    }
};

// Japanese Notes: This localization should include furigana.
CONFIG_DEFINE_LOCALE(HorizontalCamera)
{
    { ELanguage::English,  { "Horizontal Camera", "Change how the camera moves left and right." } },
    { ELanguage::Japanese, { "カメラの[左右:さゆう]", "カメラ[左右:さゆう]の\u200B[動:うご]く[方向:ほうこう]を\u200B[選択:せんたく]できます" } },
    { ELanguage::German,   { "Horizontale Kamera", "Ändere wie sich die Kamera nach links und rechts bewegt." } },
    { ELanguage::French,   { "Caméra horizontale", "Modifie la rotation horizontale de la caméra." } },
    { ELanguage::Spanish,  { "Cámara horizontal", "Cambia cómo se mueve la camara hacia la izquierda y la derecha." } },
    { ELanguage::Italian,  { "Telecamera orizzontale", "Modifica come la telecamera si muove da sinistra a destra." } }
};

// Japanese Notes: This localization should include furigana.
CONFIG_DEFINE_LOCALE(VerticalCamera)
{
    { ELanguage::English,  { "Vertical Camera", "Change how the camera moves up and down." } },
    { ELanguage::Japanese, { "カメラの[上下:じょうげ]", "カメラ[上下:じょうげ]の\u200B[動:うご]く[方向:ほうこう]を\u200B[選択:せんたく]できます" } },
    { ELanguage::German,   { "Vertikale Kamera", "Ändere wie sich die Kamera nach oben und unten bewegt." } },
    { ELanguage::French,   { "Caméra verticale", "Modifie la rotation verticale de la caméra." } },
    { ELanguage::Spanish, { "Cámara vertical", "Cambia cómo se mueve la camara hacia arriba y abajo." } },
    { ELanguage::Italian,  { "Telecamera verticale", "Modifica come la telecamera si muove su e giù." } }
};

CONFIG_DEFINE_ENUM_LOCALE(ECameraRotationMode)
{
    {
        ELanguage::English,
        {
            { ECameraRotationMode::Normal,  { "NORMAL", "" } },
            { ECameraRotationMode::Reverse, { "REVERSE", "" } }
        }
    },
    {
        ELanguage::Japanese,
        {
            { ECameraRotationMode::Normal,  { "ノーマル", "" } },
            { ECameraRotationMode::Reverse, { "リバース", "" } }
        }
    },
    {
        ELanguage::German,
        {
            { ECameraRotationMode::Normal,  { "NORMAL", "" } },
            { ECameraRotationMode::Reverse, { "INVERTIERT", "" } }
        }
    },
    {
        ELanguage::French,
        {
            { ECameraRotationMode::Normal,  { "NORMALE", "" } },
            { ECameraRotationMode::Reverse, { "INVERSÉE", "" } }
        }
    },
    {
        ELanguage::Spanish,
        {
            { ECameraRotationMode::Normal,  { "NORMAL", "" } },
            { ECameraRotationMode::Reverse, { "INVERTIDO", "" } }
        }
    },
    {
        ELanguage::Italian,
        {
            { ECameraRotationMode::Normal,  { "NORMALE", "" } },
            { ECameraRotationMode::Reverse, { "INVERTITA", "" } }
        }
    }
};

// Japanese Notes: This localization should include furigana.
CONFIG_DEFINE_LOCALE(Vibration)
{
    { ELanguage::English,  { "Vibration", "Toggle controller vibration." } },
    { ELanguage::Japanese, { "[振動:しんどう]", "[振動:しんどう]の\u200B[有無:うむ]を\u200B[選択:せんたく]できます" } },
    { ELanguage::German,   { "Vibration", "Schalte die Controllervibration an oder aus." } },
    { ELanguage::French,   { "Vibration", "Active les vibrations de la manette." } },
    { ELanguage::Spanish,  { "Vibración", "Activa o desactiva la vibración del mando." } },
    { ELanguage::Italian,  { "Vibrazione", "Attiva/disattiva la vibrazione del controller." } }
};

CONFIG_DEFINE_LOCALE(RumbleStrength)
{
    { ELanguage::English,  { "Rumble Strength", "Adjust motor-rumble strength from 0% to 100%. Used by SDL controllers and the PS5 compatible-mode fallback." } },
    { ELanguage::Japanese, { "[ランブル:らんぶる]の[強さ:つよさ]", "モーターの[振動:しんどう]の[強さ:つよさ]を0%から100%で[調整:ちょうせい]します。SDLコントローラーとPS5の[互換:ごかん]モードで[使:つか]われます。" } },
    { ELanguage::German,   { "Rumble-Stärke", "Legt die Stärke des Motor-Rumbles von 0 % bis 100 % fest. Für SDL-Controller und den PS5-Kompatibilitätsmodus." } },
    { ELanguage::French,   { "Intensité du rumble", "Règle l’intensité du rumble de 0 à 100 %. Pour les manettes SDL et le mode compatible de la PS5." } },
    { ELanguage::Spanish,  { "Intensidad del rumble", "Ajusta la intensidad del rumble del 0 % al 100 %. Para mandos SDL y el modo compatible de PS5." } },
    { ELanguage::Italian,  { "Intensità del rumble", "Regola l’intensità del rumble dallo 0% al 100%. Per i controller SDL e la modalità compatibile PS5." } }
};

CONFIG_DEFINE_LOCALE(VibrationStrength)
{
    { ELanguage::English,  { "Vibration Strength", "Adjust DualSense audio-haptic strength from 0% to 100% when the advanced type-10 route is active." } },
    { ELanguage::Japanese, { "DualSense[振動:しんどう]の[強さ:つよさ]", "DualSenseのオーディオ[振動:しんどう]の[強さ:つよさ]を0%から100%で[調整:ちょうせい]します。タイプ10の[高度:こうど]な[振動:しんどう]ルートで[使:つか]われます。" } },
    { ELanguage::German,   { "DualSense-Haptikstärke", "Legt die Stärke der DualSense-Audiohaptik von 0 % bis 100 % fest. Wird mit der erweiterten Typ-10-Route verwendet." } },
    { ELanguage::French,   { "Intensité haptique DualSense", "Règle l’intensité haptique audio DualSense de 0 à 100 % lorsque la voie avancée de type 10 est active." } },
    { ELanguage::Spanish,  { "Intensidad háptica DualSense", "Ajusta la intensidad háptica de audio del DualSense del 0 % al 100 % cuando está activa la ruta avanzada tipo 10." } },
    { ELanguage::Italian,  { "Intensità aptica DualSense", "Regola l’intensità aptica audio del DualSense dallo 0% al 100% quando è attivo il percorso avanzato di tipo 10." } }
};

CONFIG_DEFINE_LOCALE(VibrationMenu)
{
    { ELanguage::English,  { "Vibration Menu", "Toggle controller vibration in menus and strength previews." } },
    { ELanguage::Japanese, { "[メニュー:めにゅー][振動:しんどう]", "[メニュー:めにゅー]と[強:つよ]さプレビューのコントローラー[振動:しんどう]を[切:き]り[替:か]えます。" } },
    { ELanguage::German,   { "Menüvibration", "Schalte die Controllervibration in Menüs und bei Stärkevorschauen an oder aus." } },
    { ELanguage::French,   { "Vibration des menus", "Active ou désactive les vibrations de la manette dans les menus et lors de l’aperçu de l’intensité." } },
    { ELanguage::Spanish,  { "Vibración en menús", "Activa o desactiva la vibración del mando en los menús y al previsualizar la intensidad." } },
    { ELanguage::Italian,  { "Vibrazione nei menu", "Attiva o disattiva la vibrazione del controller nei menu e nell’anteprima dell’intensità." } }
};

// Japanese Notes: This localization should include furigana.
CONFIG_DEFINE_LOCALE(AllowBackgroundInput)
{
    { ELanguage::English,  { "Allow Background Input", "Allow controller input whilst the game window is unfocused." } },
    { ELanguage::Japanese, { "バックグラウンド[入力:にゅうりょく]", "フォーカスされていない\u200Bゲームに\u200B[入力:にゅうりょく]できるか\u200B[選択:せんたく]できます" } },
    { ELanguage::German,   { "Erlaube Hintergrundeingaben", "Erlaube Eingaben deines Controllers auch wenn das Spielfenster nicht fokussiert ist." } },
    { ELanguage::French,   { "Manette en arrière plan", "Permet d'utiliser la manette dans le jeu lorsque qu'il n'est pas au premier plan." } },
    { ELanguage::Spanish,  { "Control en segundo plano", "Permite controlar el juego con un mando mientras la ventana esté en segundo plano." } },
    { ELanguage::Italian,  { "Input con la finestra inattiva", "Attiva/disattiva i tasti del controller mentre la finestra è inattiva." } }
};

// Japanese Notes: This localization should include furigana.
CONFIG_DEFINE_LOCALE(MasterVolume)
{
    { ELanguage::English,  { "Master Volume", "Adjust the overall volume." } },
    { ELanguage::Japanese, { "マスターボリューム", "[全体:ぜんたい]ボリュームの\u200B[大:おお]きさを\u200B[調整:ちょうせい]できます" } },
    { ELanguage::German,   { "Gesamtlautstärke", "Passe die Gesamtlautstärke an." } },
    { ELanguage::French,   { "Volume général", "Réglage du volume général." } },
    { ELanguage::Spanish,  { "Volumen maestro", "Ajusta el volumen general." } },
    { ELanguage::Italian,  { "Volume principale", "Regola il volume principale" } }
};

// Japanese Notes: This localization should include furigana.
CONFIG_DEFINE_LOCALE(MusicVolume)
{
    { ELanguage::English,  { "Music Volume", "Adjust the volume for the music." } },
    { ELanguage::Japanese, { "BGMボリューム", "BGMボリュームの\u200B[大:おお]きさを\u200B[調整:ちょうせい]できます" } },
    { ELanguage::German,   { "Musiklautstärke", "Passe die Lautstärke der Musik an." } },
    { ELanguage::French,   { "Volume de la musique", "Réglage du volume de la musique." } },
    { ELanguage::Spanish,  { "Volumen de la música", "Ajusta el volumen de la música." } },
    { ELanguage::Italian,  { "Volume musica di sottofondo", "Regola il volume della musica di sottofondo." } }
};

// Japanese Notes: This localization should include furigana.
CONFIG_DEFINE_LOCALE(EffectsVolume)
{
    { ELanguage::English,  { "Effects Volume", "Adjust the volume for sound effects." } },
    { ELanguage::Japanese, { "SEボリューム", "SEボリュームの\u200B[大:おお]きさを\u200B[調整:ちょうせい]できます" } },
    { ELanguage::German,   { "Soundeffektlautstärke", "Passe die Lautstärke der Soundeffekte an." } },
    { ELanguage::French,   { "Volume des effets sonores", "Réglage du volume des effets sonores." } },
    { ELanguage::Spanish,  { "Volumen de efectos", "Ajusta el volumen de los efectos de sonido." } },
    { ELanguage::Italian,  { "Volume effetti sonori", "Regola il volume degli effetti sonori." } }
};

// Japanese Notes: This localization should include furigana.
CONFIG_DEFINE_LOCALE(MusicAttenuation)
{
    { ELanguage::English,  { "Music Attenuation", "Fade out the game's music when external media is playing." } },
    { ELanguage::Japanese, { "BGM[減衰:げんすい]", "[外部:がいぶ]メディアを\u200B[再生:さいせい]すると\u200Bゲームの\u200B[音楽:おんがく]を\u200Bフェードアウト\u200Bします" } },
    { ELanguage::German,   { "Musikdämpfung", "Stelle die Musik des Spiels stumm während externe Medien abgespielt werden." } },
    { ELanguage::French,   { "Atténuation audio", "Abaisse le volume des musiques du jeu lorsqu'un média externe est en cours de lecture." } },
    { ELanguage::Spanish,  { "Atenuación de música", "Atenúa la música del juego cuando un reproductor multimedia se encuentra activo." } },
    { ELanguage::Italian,  { "Attenuazione musica", "Abbassa il volume della musica di sottofondo quando un'altra applicazione riproduce dei suoni." } }
};

// Japanese Notes: This localization should include furigana.
CONFIG_DEFINE_LOCALE(ChannelConfiguration)
{
    { ELanguage::English,  { "Channel Configuration", "Change the output mode for your audio device." } },
    { ELanguage::Japanese, { "チャンネル[設定:せってい]", "オーディオデバイスの\u200B[\u2005出\u2005力\u2005:しゅつりょく]モード\u200Bを[変更:へんこう]できます" } },
    { ELanguage::German,   { "Kanalkonfiguration", "Ändere den Ausgabemodus für dein Audioausgabegerät." } },
    { ELanguage::French,   { "Configuration sortie audio", "Modifie le mode de sortie pour votre périphérique audio." } },
    { ELanguage::Spanish,  { "Configuración de canales", "Cambia el modo de salida para tu dispositivo de audio." } },
    { ELanguage::Italian,  { "Configurazione canali audio", "Modifica la modalità di output per il tuo dispositivo audio." } }
};

CONFIG_DEFINE_ENUM_LOCALE(EChannelConfiguration)
{
    {
        ELanguage::English,
        {
            { EChannelConfiguration::Stereo,   { "STEREO", "" } },
            { EChannelConfiguration::Surround, { "SURROUND", "" } }
        }
    },
    {
        ELanguage::Japanese,
        {
            { EChannelConfiguration::Stereo,   { "ステレオ", "" } },
            { EChannelConfiguration::Surround, { "サラウンド", "" } }
        }
    },
    {
        ELanguage::German,
        {
            { EChannelConfiguration::Stereo,   { "STEREO", "" } },
            { EChannelConfiguration::Surround, { "SURROUND", "" } }
        }
    },
    {
        ELanguage::French,
        {
            { EChannelConfiguration::Stereo,   { "STÉRÉO", "" } },
            { EChannelConfiguration::Surround, { "SURROUND", "" } }
        }
    },
    {
        ELanguage::Spanish,
        {
            { EChannelConfiguration::Stereo,   { "ESTÉREO", "" } },
            { EChannelConfiguration::Surround, { "ENVOLVENTE", "" } }
        }
    },
    {
        ELanguage::Italian,
        {
            { EChannelConfiguration::Stereo,   { "STEREO", "" } },
            { EChannelConfiguration::Surround, { "SURROUND", "" } }
        }
    }
};

// Japanese Notes: This localization should include furigana.
CONFIG_DEFINE_LOCALE(VoiceLanguage)
{
    { ELanguage::English,  { "Voice Language", "Change the language used for character voices." } },
    { ELanguage::Japanese, { "[音声言語:おんせいげんご]", "ゲーム[内:ない]の\u200B[音声言語:おんせいげんご]を\u200B[変更:へんこう]できます" } },
    { ELanguage::German,   { "Stimmeinstellung", "Ändere die Sprache, die für Charakterstimmen benutzt wird." } },
    { ELanguage::French,   { "Langue de doublage", "Modifie la langue utilisée pour la voix des personnages." } },
    { ELanguage::Spanish,  { "Idioma de voz", "Cambia el idioma utilizado para las voces de los personajes." } },
    { ELanguage::Italian, { "Lingua delle voci", "Modifica la lingua utilizzata per le voci dei personaggi." } }
};

// Japanese Notes: This localization should include furigana.
CONFIG_DEFINE_LOCALE(Subtitles)
{
    { ELanguage::English,  { "Subtitles", "Show subtitles during dialogue." } },
    { ELanguage::Japanese, { "[字幕:じまく]", "[字幕:じまく]の[表示:ひょうじ]を\u200B[選択:せんたく]できます" } },
    { ELanguage::German,   { "Untertitel", "Zeige Untertitel bei Dialogen." } },
    { ELanguage::French,   { "Sous-titres", "Affiche les sous-titres pendant les dialogues." } },
    { ELanguage::Spanish,  { "Subtítulos", "Muestra subtítulos durante los diálogos." } },
    { ELanguage::Italian,  { "Sottotitoli", "Mostra i sottotitoli durante i dialoghi." } }
};

// Japanese Notes: This localization should include furigana.
CONFIG_DEFINE_LOCALE(BattleTheme)
{
    { ELanguage::English,  { "Battle Theme", "Play the Werehog battle theme during combat.\n\nThis option will apply the next time you're in combat.\n\nExorcism missions and miniboss themes will be unaffected." } },
    { ELanguage::Japanese, { "バトルテーマ", "バトル[中:ちゅう]に\u200Bウェアホッグの\u200Bバトルテーマを\u200B[再生:さいせい]するか\u200B[選択:せんたく]できます\n\nこのオプションは\u200B[次回:じかい]のバトルから\u200B[適用:てきよう]されます\n\n[\u2005除\u2005霊\u2005:エクソシズム]ミッションと\u200Bミニボステーマには\u200B[\u2005影響\u2005:えいきょう]しません" } },
    { ELanguage::German,   { "Kampfmusik", "Spiele die Kampfmusik des Werehogs während dem Kämpfen ab.\n\nDiese Option tritt das nächste Mal, wenn du in einen Kampf gerätst, in Kraft.\n\nExorzismen und Mini-Bosse werden hiervon nicht beeinflusst." } },
    { ELanguage::French,   { "Thème de combat", "Joue le thème de combat du Werehog pendant ces derniers.\n\nCette option s'appliquera la prochaine fois que vous serez en combat.\n\nLes missions d'exorcisme et les thèmes des miniboss ne seront pas affectés." } },
    { ELanguage::Spanish,  { "Tema de batalla", "Reproduce el tema de batalla del Werehog durante el combate.\n\nEsta opción se aplicará la próxima vez que entres en combate.\n\nLas misiones de exorcismo y los temas de los minijefes no se verán afectados." } },
    { ELanguage::Italian,  { "Musica di combattimento", "Riproduci la musica di combattimento del Werehog quando inizi una battaglia.\n\nQuesta opzione verrà applicata la prossima volta che sei in battaglia.\n\nLa traccia musicale verrà riprodotta ugualmente nelle missioni di Esorcismo e i miniboss." } }
};

// Japanese Notes: This localization should include furigana.
CONFIG_DEFINE_LOCALE(WindowSize)
{
    { ELanguage::English,  { "Window Size", "Adjust the size of the game window in windowed mode." } },
    { ELanguage::Japanese, { "ウィンドウサイズ", "ウィンドウ\u200Bモードでの\u200Bゲームの\u200Bウィンドウサイズを\u200B[調整:ちょうせい]できます" } },
    { ELanguage::German,   { "Fenstergröße", "Ändere die Größe des Spielfensters im Fenstermodus." } },
    { ELanguage::French,   { "Taille de la fenêtre", "Modifie la taille de la fenêtre de jeu en mode fenêtré." } },
    { ELanguage::Spanish,  { "Tamaño de ventana", "Ajusta el tamaño de la ventana de juego." } },
    { ELanguage::Italian,  { "Dimensioni finestra", "Regola le dimensioni della finestra del gioco in modalità finestra." } }
};

// Japanese Notes: This localization should include furigana.
CONFIG_DEFINE_LOCALE(Monitor)
{
    { ELanguage::English,  { "Monitor", "Change which monitor to display the game on." } },
    { ELanguage::Japanese, { "モニター[選択:せんたく]", "ゲームを[表示:ひょうじ]する\u200Bモニターを\u200B[変更:へんこう]できます" } },
    { ELanguage::German,   { "Monitor", "Ändere auf welchem Monitor das Spiel angezeigt wird." } },
    { ELanguage::French,   { "Moniteur", "Change le moniteur sur lequel le jeu sera affiché." } },
    { ELanguage::Spanish,  { "Pantalla", "Cambia la pantalla en la cuál se muestra el juego." } },
    { ELanguage::Italian,  { "Schermo", "Cambia lo schermo su cui visualizzare il gioco." } }
};

// Japanese Notes: This localization should include furigana.
CONFIG_DEFINE_LOCALE(AspectRatio)
{
    { ELanguage::English,  { "Aspect Ratio", "Change the aspect ratio." } },
    { ELanguage::Japanese, { "アスペクト[比:ひ]", "アスペクト[比:ひ]を\u200B[変更:へんこう]できます" } },
    { ELanguage::German,   { "Seitenverhältnis", "Verändere das Seitenverhältnis." } },
    { ELanguage::French,   { "Format d'image", "Modifie le format d'image." } },
    { ELanguage::Spanish,  { "Relación de aspecto", "Cambia la relación de aspecto." } },
    { ELanguage::Italian,  { "Rapporto d'aspetto", "Modifica il rapporto d'aspetto." } }
};

// Japanese Notes: This localization should include furigana in its description.
CONFIG_DEFINE_ENUM_LOCALE(EAspectRatio)
{
    {
        ELanguage::English,
        {
            { EAspectRatio::Auto, { "AUTO", "Auto: the aspect ratio will dynamically adjust to the window size." } },
            { EAspectRatio::Wide, { "16:9", "16:9: locks the game to a widescreen aspect ratio." } },
            { EAspectRatio::Narrow, { "4:3", "4:3: locks the game to a narrow aspect ratio." } },
            { EAspectRatio::OriginalNarrow, { "ORIGINAL 4:3", "Original 4:3: locks the game to a narrow aspect ratio and retains parity with the game's original implementation." } }
        }
    },
    {
        ELanguage::Japanese,
        {
            { EAspectRatio::Auto, { "自動", "[自動:じどう]: アスペクト[比:ひ]は\u200Bウィンドウサイズに\u200B[合:あ]わせて\u200B[調整:ちょうせい]されます" } },
            { EAspectRatio::Wide, { "16:9", "16:9: ワイドスクリーンの\u200Bアスペクト[比:ひ]に\u200B[固定:こてい]されます" } },
            { EAspectRatio::Narrow, { "4:3", "4:3: ナローの\u200Bアスペクト[比:ひ]に\u200B[固定:こてい]されます" } },
            { EAspectRatio::OriginalNarrow, { "オリジナル 4:3", "オリジナル 4:3: オリジナルの\u200Bアスペクト[比:ひ]に\u200B[固定:こてい]されます" } }
        }
    },
    {
        ELanguage::German,
        {
            { EAspectRatio::Auto, { "AUTO", "Auto: Das Seitenverhältnis passt sich automatisch der Fenstergröße an." } },
            { EAspectRatio::Wide, { "16:9", "16:9: Stellt das Spiel in einem Breitbildschirm-Format dar." } },
            { EAspectRatio::Narrow, { "4:3", "4:3: Stellt das Spiel in einem Mittel-Format dar." } },
            { EAspectRatio::OriginalNarrow, { "ORIGINAL 4:3", "Original 4:3: Stellt das Spiel in einem Mittel-Format dar, was der ursprünglichen Implementation originalgetreut bleibt." } }
        }
    },
    {
        ELanguage::French,
        {
            { EAspectRatio::Auto, { "AUTO", "Auto : le format d'image s'adapte automatiquement à la taille de la fenêtre." } },
            { EAspectRatio::Wide, { "16:9", "16:9 : force le jeu sur un format d'image large." } },
            { EAspectRatio::Narrow, { "4:3", "4:3 : force le jeu sur un format d'image carré." } },
            { EAspectRatio::OriginalNarrow, { "4:3 ORIGINAL", "4:3 original : force le jeu à un format d'image carré et conserve la parité avec l'implémentation d'origine du jeu." } }
        }
    },
    {
        ELanguage::Spanish,
        {
            { EAspectRatio::Auto, { "AUTO", "Auto: la relación de aspecto se ajusta de forma dinámica al tamaño de la ventana." } },
            { EAspectRatio::Wide, { "16:9", "16:9: muestra el juego en relación de aspecto de pantalla ancha." } },
            { EAspectRatio::Narrow, { "4:3", "4:3: muestra el juego en relación de aspecto de pantalla estrecha." } },
            { EAspectRatio::OriginalNarrow, { "ORIGINAL 4:3", "Original 4:3: muestra el juego en relación de aspecto de pantalla estrecha y mantiene paridad con la implementación original del juego." } }
        }
    },
    {
        ELanguage::Italian,
        {
            { EAspectRatio::Auto, { "AUTO", "Auto: il rapporto d'aspetto verra cambiato automaticamente in base alle dimensioni della finestra." } },
            { EAspectRatio::Wide, { "16:9", "16:9: blocca il gioco a un rapporto d'aspetto widescreen." } },
            { EAspectRatio::Narrow, { "4:3", "4:3: blocca il gioco a un rapporto d'aspetto stretto." } },
            { EAspectRatio::OriginalNarrow, { "4:3 ORIGINALE", "4:3 Originale: blocca il gioco a un rapporto d'aspetto stretto e mantiene la parità con l'implementazione originale del gioco." } }
        }
    }
};

// Japanese Notes: This localization should include furigana.
CONFIG_DEFINE_LOCALE(ResolutionScale)
{
    { ELanguage::English,  { "Resolution Scale", "Adjust the internal resolution of the game.\n\n%dx%d" } },
    { ELanguage::Japanese, { "[解像度:かいぞうど]スケール", "ゲームの\u200B[内部解像度:ないぶかいぞうど]を\u200B[調整:ちょうせい]できます\n\n%dx%d" } },
    { ELanguage::German,   { "Rendering-Auflösung", "Passe die Auflösung der internen Darstellung an.\n\n%dx%d" } },
    { ELanguage::French,   { "Échelle de rendu", "Modifie la résolution interne du jeu.\n\n%dx%d" } },
    { ELanguage::Spanish,  { "Escala de resolución", "Ajusta la resolución interna del juego.\n\n%dx%d" } },
    { ELanguage::Italian, { "Scala risoluzione", "Regola la risoluzione interna del gioco.\n\n%dx%d" } }
};

// Japanese Notes: This localization should include furigana.
CONFIG_DEFINE_LOCALE(Fullscreen)
{
    { ELanguage::English,  { "Fullscreen", "Toggle between borderless fullscreen or windowed mode." } },
    { ELanguage::Japanese, { "フルスクリーン", "ボーダーレス\u200Bフルスクリーンか\u200Bウィンドウモードを\u200B[選択:せんたく]できます" } },
    { ELanguage::German,   { "Vollbild", "Wechsle zwischen dem randlosen Vollbildmodus und dem Fenstermodus." } },
    { ELanguage::French,   { "Plein écran", "Alterne entre le mode plein écran sans bordures et le mode fenêtré." } },
    { ELanguage::Spanish,  { "Pantalla completa", "Cambia entre modo de pantalla completa o ventana." } },
    { ELanguage::Italian,  { "Schermo pieno", "Attiva/disattiva tra modalità finestra senza cornice e modalità finestra." } }
};

// Japanese Notes: This localization should include furigana.
CONFIG_DEFINE_LOCALE(VSync)
{
    { ELanguage::English,  { "V-Sync", "Synchronize the game to the refresh rate of the display to prevent screen tearing." } },
    { ELanguage::Japanese, { "[垂直同期:すいちょくどうき]", "[垂直同期:すいちょくどうき]の\u200B[設定:せってい]を\u200B[変更:へんこう]できます" } },
    { ELanguage::German,   { "V-Sync", "Synchronisiere das Spiel mit der Bildwiederholrate deines Bildschirms um Bildverzerrungen zu vermeiden." } },
    { ELanguage::French,   { "V-Sync", "Synchronise le jeu avec la fréquence de rafraîchissement de l'écran pour éviter le screen tearing." } },
    { ELanguage::Spanish,  { "V-Sync", "Sincroniza el juego a la tasa de refresco de la pantalla para prevenir el rasgado de la imagen." } },
    { ELanguage::Italian,  { "V-Sync", "Sincronizza il gioco con la frequenza d'aggiornamento del display per evitare lo screen tearing." } }
};

// Japanese Notes: This localization should include furigana.
CONFIG_DEFINE_LOCALE(FPS)
{
    { ELanguage::English,  { "FPS", "Set the maximum frame rate. PS5 choices are 30, 60, 90, and 120 FPS; the default remains 60 FPS." } },
    { ELanguage::Japanese, { "フレームレート[上限:じょうげん]", "ゲームの[最大:さいだい]フレームレートを[設定:せってい]します。PS5では30、60、90、120 FPSから[選択:せんたく]できます。[初期設定:しょきせってい]は60 FPSです" } },
    { ELanguage::German,   { "FPS", "Lege die maximale Bildrate fest. Auf PS5 stehen 30, 60, 90 und 120 FPS zur Auswahl; der Standard bleibt 60 FPS." } },
    { ELanguage::French,   { "IPS", "Définit la limite d'images par seconde. Sur PS5, choisissez 30, 60, 90 ou 120 IPS ; la valeur par défaut reste 60 IPS." } },
    { ELanguage::Spanish,  { "FPS", "Establece el límite de fotogramas. En PS5 puedes elegir 30, 60, 90 o 120 FPS; el valor predeterminado sigue siendo 60 FPS." } },
    { ELanguage::Italian,  { "FPS", "Imposta il limite di frame rate. Su PS5 puoi scegliere 30, 60, 90 o 120 FPS; il valore predefinito resta 60 FPS." } }
};

// Japanese Notes: This localization should include furigana.
CONFIG_DEFINE_LOCALE(AnisotropicFiltering)
{
    { ELanguage::English,  { "Anisotropic Filtering", "Choose None (0), 2x, 4x, 8x, or 16x anisotropic filtering. 16x is the default; None disables it." } },
    { ELanguage::Japanese, { "[異方性:いほうせい]フィルタリング", "なし (0)、2x、4x、8x、16xから[選択:せんたく]します。16xが[既定値:きていち]です" } },
    { ELanguage::German,   { "Anisotrope Filterung", "Wähle Keine (0), 2x, 4x, 8x oder 16x anisotrope Filterung. 16x ist der Standard; Keine deaktiviert sie." } },
    { ELanguage::French,   { "Filtrage anisotrope", "Choisissez Aucun (0), 2x, 4x, 8x ou 16x. Le réglage par défaut est 16x ; Aucun désactive le filtrage." } },
    { ELanguage::Spanish,  { "Filtrado anisotrópico", "Elige Ninguno (0), 2x, 4x, 8x o 16x. El valor predeterminado es 16x; Ninguno desactiva el filtrado." } },
    { ELanguage::Italian,  { "Filtro anisotropico", "Scegli Nessuno (0), 2x, 4x, 8x o 16x. Il valore predefinito è 16x; Nessuno disattiva il filtro." } }
};

// Japanese Notes: This localization should include furigana.
CONFIG_DEFINE_LOCALE(DepthOfFieldQuality)
{
    { ELanguage::English,  { "Depth of Field Quality", "Choose the depth-of-field blur quality. Lower settings use smaller blur kernels; Auto preserves the game's resolution-based choice." } },
    { ELanguage::Japanese, { "[被写界深度:ひしゃかいしんど]の[品質:ひんしつ]", "[被写界深度:ひしゃかいしんど]のぼかし[品質:ひんしつ]を[選択:せんたく]します。[低:ひく]い[設定:せってい]ではぼかしカーネルが[小:ちい]さくなり、[自動:じどう]ではゲームの[解像度:かいぞうど]に[応:おう]じた[選択:せんたく]が[維持:いじ]されます" } },
    { ELanguage::German,   { "Tiefenschärfequalität", "Legt die Qualität der Tiefenschärfe fest. Niedrigere Einstellungen verwenden kleinere Unschärfekerne; Auto behält die auflösungsabhängige Spielauswahl bei." } },
    { ELanguage::French,   { "Qualité de profondeur de champ", "Choisissez la qualité du flou de profondeur de champ. Les réglages bas utilisent des noyaux plus petits ; Auto conserve le choix du jeu selon la résolution." } },
    { ELanguage::Spanish,  { "Calidad de profundidad de campo", "Elige la calidad del desenfoque de profundidad de campo. Los ajustes bajos usan kernels menores; Automático conserva la elección del juego según la resolución." } },
    { ELanguage::Italian,  { "Qualità profondità di campo", "Scegli la qualità della sfocatura della profondità di campo. Le impostazioni basse usano kernel più piccoli; Auto mantiene la scelta del gioco in base alla risoluzione." } }
};

// Japanese Notes: This localization should include furigana.
CONFIG_DEFINE_ENUM_LOCALE(EDepthOfFieldQuality)
{
    {
        ELanguage::English,
        {
            { EDepthOfFieldQuality::Auto,   { "AUTO",   "Automatic: use the game's default depth-of-field quality." } },
            { EDepthOfFieldQuality::Low,    { "LOW",    "Low: use a smaller blur kernel to reduce depth-of-field work." } },
            { EDepthOfFieldQuality::Medium, { "MEDIUM", "Medium depth-of-field quality." } },
            { EDepthOfFieldQuality::High,   { "HIGH",   "High depth-of-field quality." } },
            { EDepthOfFieldQuality::Ultra,  { "ULTRA",  "Ultra depth-of-field quality." } }
        }
    },
    {
        ELanguage::Japanese,
        {
            { EDepthOfFieldQuality::Auto,   { "[自動:じどう]", "[自動:じどう]: ゲームの[既定:きてい]の[被写界深度:ひしゃかいしんど]を[使用:しよう]します" } },
            { EDepthOfFieldQuality::Low,    { "[低:てい]", "[低:てい]: [小:ちい]さいぼかしカーネルを[使:つか]い、[被写界深度:ひしゃかいしんど]の[処理負荷:しょりふか]を[減:へ]らします" } },
            { EDepthOfFieldQuality::Medium, { "[中:ちゅう]", "[中:ちゅう]の[被写界深度:ひしゃかいしんど]の[品質:ひんしつ]です" } },
            { EDepthOfFieldQuality::High,   { "[高:こう]", "[高:こう]い[被写界深度:ひしゃかいしんど]の[品質:ひんしつ]です" } },
            { EDepthOfFieldQuality::Ultra,  { "[最高:さいこう]", "[最高:さいこう]の[被写界深度:ひしゃかいしんど]の[品質:ひんしつ]です" } }
        }
    },
    {
        ELanguage::German,
        {
            { EDepthOfFieldQuality::Auto,   { "AUTO",   "Automatisch: Die standardmäßige Tiefenschärfe des Spiels wird verwendet." } },
            { EDepthOfFieldQuality::Low,    { "NIEDRIG", "Niedrig: Ein kleinerer Unschärfekernel reduziert den Aufwand für Tiefenschärfe." } },
            { EDepthOfFieldQuality::Medium, { "MITTEL", "Mittlere Tiefenschärfequalität." } },
            { EDepthOfFieldQuality::High,   { "HOCH",   "Hohe Tiefenschärfequalität." } },
            { EDepthOfFieldQuality::Ultra,  { "ULTRA",  "Höchste Tiefenschärfequalität." } }
        }
    },
    {
        ELanguage::French,
        {
            { EDepthOfFieldQuality::Auto,   { "AUTO",   "Automatique : utilise la qualité de profondeur de champ par défaut du jeu." } },
            { EDepthOfFieldQuality::Low,    { "BASSE",  "Basse : un noyau de flou réduit le coût du traitement de la profondeur de champ." } },
            { EDepthOfFieldQuality::Medium, { "MOYENNE", "Qualité moyenne de profondeur de champ." } },
            { EDepthOfFieldQuality::High,   { "ÉLEVÉE", "Qualité élevée de profondeur de champ." } },
            { EDepthOfFieldQuality::Ultra,  { "ULTRA",  "Qualité ultra de profondeur de champ." } }
        }
    },
    {
        ELanguage::Spanish,
        {
            { EDepthOfFieldQuality::Auto,   { "AUTO",   "Automático: usa la calidad de profundidad de campo predeterminada del juego." } },
            { EDepthOfFieldQuality::Low,    { "BAJA",   "Baja: usa un desenfoque menor para reducir el trabajo de profundidad de campo." } },
            { EDepthOfFieldQuality::Medium, { "MEDIA",  "Calidad media de profundidad de campo." } },
            { EDepthOfFieldQuality::High,   { "ALTA",   "Calidad alta de profundidad de campo." } },
            { EDepthOfFieldQuality::Ultra,  { "ULTRA",  "Calidad ultra de profundidad de campo." } }
        }
    },
    {
        ELanguage::Italian,
        {
            { EDepthOfFieldQuality::Auto,   { "AUTO",   "Automatico: usa la qualità predefinita della profondità di campo del gioco." } },
            { EDepthOfFieldQuality::Low,    { "BASSA",  "Bassa: usa un kernel di sfocatura più piccolo per ridurre il lavoro sulla profondità di campo." } },
            { EDepthOfFieldQuality::Medium, { "MEDIA",  "Qualità media della profondità di campo." } },
            { EDepthOfFieldQuality::High,   { "ALTA",   "Qualità alta della profondità di campo." } },
            { EDepthOfFieldQuality::Ultra,  { "ULTRA",  "Qualità ultra della profondità di campo." } }
        }
    }
};

// Japanese Notes: This localization should include furigana.
CONFIG_DEFINE_LOCALE(ShowSystemInfo)
{
    { ELanguage::English,  { "Enable System Info Overlay", "Show the game title and FPS. Choose Simple to show only these, or Advanced to enable additional metrics and per-item visibility switches." } },
    { ELanguage::Japanese, { "システム[情報:じょうほう]パネルを[有効:ゆうこう]にする", "ゲーム[名:めい]とFPSを[表示:ひょうじ]します。シンプルではこの2[項目:こうもく]のみ、アドバンスでは[追加:ついか]の[情報:じょうほう]と[項目:こうもく]ごとの[表示設定:ひょうじせってい]を[使用:しよう]します。" } },
    { ELanguage::German,   { "Systeminfo-Overlay aktivieren", "Zeigt Spieltitel und FPS. Einfach zeigt nur diese beiden; Erweitert bietet zusätzliche Messwerte und Ein-/Aus-Schalter pro Eintrag." } },
    { ELanguage::French,   { "Activer le panneau système", "Affiche le titre du jeu et les IPS. Simple n'affiche que ces éléments ; Avancé ajoute des mesures et des interrupteurs individuels." } },
    { ELanguage::Spanish,  { "Activar panel de información", "Muestra el título del juego y los FPS. Simple muestra solo estos elementos; Avanzado añade métricas y controles individuales de visibilidad." } },
    { ELanguage::Italian,  { "Attiva pannello info di sistema", "Mostra il titolo del gioco e gli FPS. Semplice mostra solo questi elementi; Avanzata aggiunge metriche e interruttori di visibilità individuali." } }
};

// Japanese Notes: This localization should include furigana.
CONFIG_DEFINE_LOCALE(SystemInfoAccentPreset)
{
    { ELanguage::English,  { "Overlay Accent Color", "Choose from 256 color presets for the overlay. Press A to edit; use the D-pad to select a swatch and L1/R1 to change pages. Press A to confirm or B to cancel." } },
    { ELanguage::Japanese, { "オーバーレイのアクセント[色:いろ]", "オーバーレイの[色:いろ]を256[色:しょく]のプリセットから[選択:せんたく]します。Aボタンで[編集:へんしゅう]し、[方向:ほうこう]キーで[色:いろ]を[選択:せんたく]、L1/R1でページを[切替:きりか]えます。Aボタンで[確定:かくてい]、Bボタンで[取消:とりけし]します。" } },
    { ELanguage::German,   { "Overlay-Akzentfarbe", "Wähle für das Overlay aus 256 Farbvorlagen. Mit A bearbeiten, per Steuerkreuz eine Farbe wählen und mit L1/R1 die Seiten wechseln. Mit A bestätigen oder mit B abbrechen." } },
    { ELanguage::French,   { "Couleur d’accent de l’overlay", "Choisissez parmi 256 couleurs prédéfinies pour l’overlay. Appuyez sur A pour modifier, utilisez la croix pour choisir et L1/R1 pour changer de page. A confirme, B annule." } },
    { ELanguage::Spanish,  { "Color de acento de la superposición", "Elige entre 256 colores predefinidos para la superposición. Pulsa A para editar; usa la cruceta para elegir y L1/R1 para cambiar de página. A confirma y B cancela." } },
    { ELanguage::Italian,  { "Colore accento overlay", "Scegli tra 256 colori predefiniti per l'overlay. Premi A per modificare; usa la croce direzionale per scegliere e L1/R1 per cambiare pagina. A conferma, B annulla." } }
};

// Japanese Notes: This localization should include furigana.
CONFIG_DEFINE_LOCALE(OverlayMode)
{
    { ELanguage::English,  { "Overlay Mode", "Simple shows only the game title and FPS. Advanced shows the extra items enabled below." } },
    { ELanguage::Japanese, { "オーバーレイ[表示:ひょうじ]モード", "シンプルはゲーム[名:めい]とFPSのみ、アドバンスは[下:した]で[有効:ゆうこう]にした[項目:こうもく]も[表示:ひょうじ]します。" } },
    { ELanguage::German,   { "Overlay-Modus", "Einfach zeigt nur Spieltitel und FPS. Erweitert zeigt zusätzlich die unten aktivierten Einträge." } },
    { ELanguage::French,   { "Mode du panneau", "Simple affiche uniquement le titre du jeu et les IPS. Avancé affiche aussi les éléments activés ci-dessous." } },
    { ELanguage::Spanish,  { "Modo de superposición", "Simple muestra solo el título del juego y los FPS. Avanzado también muestra los elementos activados abajo." } },
    { ELanguage::Italian,  { "Modalità overlay", "Semplice mostra solo titolo del gioco e FPS. Avanzata mostra anche gli elementi attivati qui sotto." } }
};

CONFIG_DEFINE_ENUM_LOCALE(EOverlayDisplayMode)
{
    {
        ELanguage::English,
        {
            { EOverlayDisplayMode::Simple,   { "SIMPLE", "Game title and FPS only." } },
            { EOverlayDisplayMode::Advanced, { "ADVANCED", "Shows enabled overlay items." } }
        }
    },
    {
        ELanguage::Japanese,
        {
            { EOverlayDisplayMode::Simple,   { "シンプル", "ゲーム[名:めい]とFPSのみを[表示:ひょうじ]します。" } },
            { EOverlayDisplayMode::Advanced, { "アドバンス", "[有効:ゆうこう]なオーバーレイ[項目:こうもく]を[表示:ひょうじ]します。" } }
        }
    },
    {
        ELanguage::German,
        {
            { EOverlayDisplayMode::Simple,   { "EINFACH", "Nur Spieltitel und FPS." } },
            { EOverlayDisplayMode::Advanced, { "ERWEITERT", "Zeigt aktivierte Overlay-Einträge." } }
        }
    },
    {
        ELanguage::French,
        {
            { EOverlayDisplayMode::Simple,   { "SIMPLE", "Titre du jeu et IPS uniquement." } },
            { EOverlayDisplayMode::Advanced, { "AVANCÉ", "Affiche les éléments activés." } }
        }
    },
    {
        ELanguage::Spanish,
        {
            { EOverlayDisplayMode::Simple,   { "SIMPLE", "Solo título del juego y FPS." } },
            { EOverlayDisplayMode::Advanced, { "AVANZADO", "Muestra los elementos activados." } }
        }
    },
    {
        ELanguage::Italian,
        {
            { EOverlayDisplayMode::Simple,   { "SEMPLICE", "Solo titolo del gioco e FPS." } },
            { EOverlayDisplayMode::Advanced, { "AVANZATA", "Mostra gli elementi attivati." } }
        }
    }
};

// Japanese Notes: This localization should include furigana.
CONFIG_DEFINE_LOCALE(OverlayShowCPUUpdateTime)
{
    { ELanguage::English,  { "CPU Update Time", "Show the game's CPU update time; this is not CPU utilization." } },
    { ELanguage::Japanese, { "CPU[更新時間:こうしんじかん]", "ゲームのCPU[更新時間:こうしんじかん]を[表示:ひょうじ]します。CPU[使用率:しようりつ]ではありません。" } },
    { ELanguage::German,   { "CPU-Aktualisierungszeit", "Zeigt die CPU-Aktualisierungszeit des Spiels, nicht die CPU-Auslastung." } },
    { ELanguage::French,   { "Temps de mise à jour CPU", "Affiche le temps de mise à jour du CPU du jeu, pas son utilisation." } },
    { ELanguage::Spanish,  { "Tiempo de actualización de CPU", "Muestra el tiempo de actualización de la CPU del juego, no su uso." } },
    { ELanguage::Italian,  { "Tempo aggiornamento CPU", "Mostra il tempo di aggiornamento della CPU del gioco, non l'utilizzo." } }
};

CONFIG_DEFINE_LOCALE(OverlayShowGPUName)
{
    { ELanguage::English,  { "GPU Name", "Show the detected graphics processor name." } },
    { ELanguage::Japanese, { "GPU[名:な]", "[検出:けんしゅつ]したGPUの[名前:なまえ]を[表示:ひょうじ]します。" } },
    { ELanguage::German,   { "GPU-Name", "Zeigt den erkannten Namen des Grafikprozessors." } },
    { ELanguage::French,   { "Nom du GPU", "Affiche le nom détecté du processeur graphique." } },
    { ELanguage::Spanish,  { "Nombre de GPU", "Muestra el nombre detectado del procesador gráfico." } },
    { ELanguage::Italian,  { "Nome GPU", "Mostra il nome rilevato del processore grafico." } }
};

CONFIG_DEFINE_LOCALE(OverlayShowGPUFrameTime)
{
    { ELanguage::English,  { "GPU Frame Time", "Show the time taken to render a GPU frame." } },
    { ELanguage::Japanese, { "GPUフレーム[時間:じかん]", "GPUフレームの[描画時間:びょうがじかん]を[表示:ひょうじ]します。" } },
    { ELanguage::German,   { "GPU-Framezeit", "Zeigt die Zeit zum Rendern eines GPU-Frames." } },
    { ELanguage::French,   { "Temps d'image GPU", "Affiche le temps nécessaire au rendu d'une image GPU." } },
    { ELanguage::Spanish,  { "Tiempo de fotograma GPU", "Muestra el tiempo necesario para renderizar un fotograma de GPU." } },
    { ELanguage::Italian,  { "Tempo frame GPU", "Mostra il tempo necessario per renderizzare un frame GPU." } }
};

CONFIG_DEFINE_LOCALE(OverlayShowSoCSensors)
{
    { ELanguage::English,  { "Other SoC Sensors", "Show readings from supported SoC sensor IDs other than sensor 0." } },
    { ELanguage::Japanese, { "[その他:そのた]のSoCセンサー", "センサー0[以外:いがい]の[対応:たいおう]SoCセンサー[値:ち]を[表示:ひょうじ]します。" } },
    { ELanguage::German,   { "Weitere SoC-Sensoren", "Zeigt Werte unterstützter SoC-Sensoren außer Sensor 0." } },
    { ELanguage::French,   { "Autres capteurs SoC", "Affiche les valeurs des capteurs SoC disponibles, sauf le capteur 0." } },
    { ELanguage::Spanish,  { "Otros sensores SoC", "Muestra las lecturas de los sensores SoC compatibles, excepto el sensor 0." } },
    { ELanguage::Italian,  { "Altri sensori SoC", "Mostra i valori dei sensori SoC supportati, escluso il sensore 0." } }
};

CONFIG_DEFINE_LOCALE(OverlayShowGameHeap)
{
    { ELanguage::English,  { "Game RAM Heap", "Show current, peak, and capacity values for the game allocator." } },
    { ELanguage::Japanese, { "ゲームRAMヒープ", "ゲーム用アロケーターの[現在:げんざい]・ピーク・[容量:ようりょう]を[表示:ひょうじ]します。" } },
    { ELanguage::German,   { "Spiel-RAM-Heap", "Zeigt aktuelle, Spitzen- und Kapazitätswerte des Spiel-Allokators." } },
    { ELanguage::French,   { "Heap RAM du jeu", "Affiche les valeurs actuelle, maximale et la capacité de l'allocateur du jeu." } },
    { ELanguage::Spanish,  { "Heap de RAM del juego", "Muestra los valores actual, máximo y la capacidad del asignador del juego." } },
    { ELanguage::Italian,  { "Heap RAM di gioco", "Mostra i valori attuale, massimo e la capacità dell'allocatore del gioco." } }
};

CONFIG_DEFINE_LOCALE(OverlayShowPhysicalHeap)
{
    { ELanguage::English,  { "Physical RAM Heap", "Show current, peak, and capacity values for the physical-memory allocator." } },
    { ELanguage::Japanese, { "[物理:ぶつり]RAMヒープ", "[物理:ぶつり]メモリアロケーターの[現在:げんざい]・ピーク・[容量:ようりょう]を[表示:ひょうじ]します。" } },
    { ELanguage::German,   { "Physischer RAM-Heap", "Zeigt aktuelle, Spitzen- und Kapazitätswerte des physischen Speicher-Allokators." } },
    { ELanguage::French,   { "Heap RAM physique", "Affiche les valeurs actuelle, maximale et la capacité de l'allocateur mémoire physique." } },
    { ELanguage::Spanish,  { "Heap de RAM física", "Muestra los valores actual, máximo y la capacidad del asignador de memoria física." } },
    { ELanguage::Italian,  { "Heap RAM fisica", "Mostra i valori attuale, massimo e la capacità dell'allocatore di memoria fisica." } }
};

CONFIG_DEFINE_LOCALE(OverlayShowRenderResolution)
{
    { ELanguage::English,  { "Render Resolution", "Show the current internal render resolution." } },
    { ELanguage::Japanese, { "[描画:びょうが][解像度:かいぞうど]", "[現在:げんざい]の[内部:ないぶ]レンダリング[解像度:かいぞうど]を[表示:ひょうじ]します。" } },
    { ELanguage::German,   { "Renderauflösung", "Zeigt die aktuelle interne Renderauflösung." } },
    { ELanguage::French,   { "Résolution de rendu", "Affiche la résolution de rendu interne actuelle." } },
    { ELanguage::Spanish,  { "Resolución de renderizado", "Muestra la resolución interna de renderizado actual." } },
    { ELanguage::Italian,  { "Risoluzione rendering", "Mostra la risoluzione interna di rendering corrente." } }
};

CONFIG_DEFINE_LOCALE(OverlayShowUserDataPath)
{
    { ELanguage::English,  { "User Data Path", "Show the directory used for user data and settings." } },
    { ELanguage::Japanese, { "ユーザーデータのパス", "ユーザーデータと[設定:せってい]に[使用:しよう]するフォルダーを[表示:ひょうじ]します。" } },
    { ELanguage::German,   { "Benutzerdatenpfad", "Zeigt den Ordner für Benutzerdaten und Einstellungen." } },
    { ELanguage::French,   { "Chemin des données utilisateur", "Affiche le dossier utilisé pour les données utilisateur et les paramètres." } },
    { ELanguage::Spanish,  { "Ruta de datos de usuario", "Muestra la carpeta usada para los datos y la configuración del usuario." } },
    { ELanguage::Italian,  { "Percorso dati utente", "Mostra la cartella usata per dati utente e impostazioni." } }
};

CONFIG_DEFINE_LOCALE(OverlayShowExecutablePath)
{
    { ELanguage::English,  { "Game Executable Path", "Show the path to the running game executable." } },
    { ELanguage::Japanese, { "ゲーム[実行:じっこう]ファイルのパス", "[実行中:じっこうちゅう]のゲーム[実行:じっこう]ファイルのパスを[表示:ひょうじ]します。" } },
    { ELanguage::German,   { "Pfad der Spieldatei", "Zeigt den Pfad zur laufenden Spieldatei." } },
    { ELanguage::French,   { "Chemin de l'exécutable", "Affiche le chemin de l'exécutable du jeu en cours." } },
    { ELanguage::Spanish,  { "Ruta del ejecutable del juego", "Muestra la ruta del ejecutable del juego en ejecución." } },
    { ELanguage::Italian,  { "Percorso eseguibile del gioco", "Mostra il percorso dell'eseguibile del gioco in esecuzione." } }
};

CONFIG_DEFINE_LOCALE(OverlayShowGameMount)
{
    { ELanguage::English,  { "Game Mount Path", "Show the game installation root or mount path." } },
    { ELanguage::Japanese, { "ゲームマウントのパス", "ゲームのインストール[先:さき]またはマウントのパスを[表示:ひょうじ]します。" } },
    { ELanguage::German,   { "Spiel-Mount-Pfad", "Zeigt den Installations- oder Mount-Pfad des Spiels." } },
    { ELanguage::French,   { "Chemin de montage du jeu", "Affiche le chemin d'installation ou de montage du jeu." } },
    { ELanguage::Spanish,  { "Ruta de montaje del juego", "Muestra la ruta de instalación o montaje del juego." } },
    { ELanguage::Italian,  { "Percorso mount del gioco", "Mostra il percorso di installazione o mount del gioco." } }
};

// Japanese Notes: This localization should include furigana.
CONFIG_DEFINE_LOCALE(Brightness)
{
    { ELanguage::English,  { "Brightness", "Adjust the brightness level until the symbol on the left is barely visible." } },
    { ELanguage::Japanese, { "[明:あか]るさの[設定:せってい]", "[画面:がめん]の\u200B[明:あか]るさを\u200B[調整:ちょうせい]できます" } },
    { ELanguage::German,   { "Helligkeit", "Passe die Helligkeit des Spiels an bis das linke Symbol noch gerade so sichtbar ist." } },
    { ELanguage::French,   { "Luminosité", "Réglez le niveau de luminosité jusqu'à ce que le symbole à gauche soit à peine visible." } },
    { ELanguage::Spanish,  { "Brillo", "Ajusta el nivel de brillo hasta que el símbolo a la izquierda sea apenas visible." } },
    { ELanguage::Italian,  { "Luminosità", "Regola la luminosità dello schermo fino a quando il simbolo a sinistra diventa leggermente visibile." } }
};

// Japanese Notes: This localization should include furigana.
CONFIG_DEFINE_LOCALE(AntiAliasing)
{
    { ELanguage::English,  { "Anti-Aliasing", "Adjust the amount of smoothing applied to jagged edges." } },
    { ELanguage::Japanese, { "アンチエイリアス", "アンチエイリアスの\u200B[種類:しゅるい]を\u200B[選択:せんたく]できます" } },
    { ELanguage::German,   { "Kantenglättung", "Passe die Menge an Kantenglättung an." } },
    { ELanguage::French,   { "Anticrénelage", "Ajuste le niveau d'anticrénelage appliqué aux bords des objets." } },
    { ELanguage::Spanish,  { "Anti-Aliasing", "Ajusta el nivel de suavizado aplicado a los dientes de sierra." } },
    { ELanguage::Italian,  { "Anti-Aliasing", "Regola la quantità di smussamento applicata ai bordi." } }
};

CONFIG_DEFINE_ENUM_LOCALE(EAntiAliasing)
{
    {
        ELanguage::English,
        {
            { EAntiAliasing::None, { "NONE", "" } }
        }
    },
    {
        ELanguage::Japanese,
        {
            { EAntiAliasing::None, { "オフ", "" } },
        }
    },
    {
        ELanguage::German,
        {
            { EAntiAliasing::None, { "KEINE", "" } }
        }
    },
    {
        ELanguage::French,
        {
            { EAntiAliasing::None, { "SANS", "" } }
        }
    },
    {
        ELanguage::Spanish,
        {
            { EAntiAliasing::None, { "NO", "" } },
        }
    },
    {
        ELanguage::Italian,
        {
            { EAntiAliasing::None, { "NULLO", "" } }
        }
    },
};

// Japanese Notes: This localization should include furigana.
CONFIG_DEFINE_LOCALE(TransparencyAntiAliasing)
{
    { ELanguage::English,  { "Transparency Anti-Aliasing", "Apply anti-aliasing to alpha transparent textures." } },
    { ELanguage::Japanese, { "[透明度:とうめいど]のアンチエイリアス", "[透過:とうか]テクスチャに\u200Bアンチエイリアスを\u200B[適用:てきよう]します" } },
    { ELanguage::German,   { "Transparenz-Kantenglättung", "Wende Kantenglättung auf Alpha-Transparenz-Texturen an." } },
    { ELanguage::French,   { "Anticrénelage de transparence", "Applique l'anticrénelage sur les textures transparentes." } },
    { ELanguage::Spanish,  { "Anti-Aliasing de transparencias", "Aplica antialiasing a las texturas transparentes." } },
    { ELanguage::Italian,  { "Anti-Aliasing su texture trasparenti", "Applica l'anti-aliasing alle texture trasparenti." } }
};

// Japanese Notes: This localization should include furigana.
CONFIG_DEFINE_LOCALE(ShadowResolution)
{
    { ELanguage::English,  { "Shadow Resolution", "Set the resolution of real-time shadows." } },
    { ELanguage::Japanese, { "[影:かげ]の[解像度:かいぞうど]", "[影:かげ]の[解像度:かいぞうど]を\u200B[設定:せってい]できます" } },
    { ELanguage::German,   { "Schattenauflösung", "Stelle die Auflösung der Echtzeit-Schatten ein." } },
    { ELanguage::French,   { "Résolution des ombres", "Définit la résolution des ombres en temps réel." } },
    { ELanguage::Spanish,  { "Resolución de sombras", "Establece la resolución de las sombras de tiempo real." } },
    { ELanguage::Italian,  { "Risoluzione ombre", "Imposta la risoluzioni delle ombre in tempo reale." } }
};

// Japanese Notes: This localization should include furigana in its description.
CONFIG_DEFINE_ENUM_LOCALE(EShadowResolution)
{
    {
        ELanguage::English,
        {
            { EShadowResolution::Original, { "ORIGINAL", "Original: the game will automatically determine the resolution of the shadows." } }
        }
    },
    {
        ELanguage::Japanese,
        {
            { EShadowResolution::Original, { "オリジナル", "オリジナル: [影:かげ]の[解像度:かいぞうど]が\u200B[自動的:じどうてき]に\u200B[決定:けってい]されます" } }
        }
    },
    {
        ELanguage::German,
        {
            { EShadowResolution::Original, { "ORIGINAL", "Original: Das Spiel bestimmt die Auflösung der Schatten automatisch." } }
        }
    },
    {
        ELanguage::French,
        {
            { EShadowResolution::Original, { "ORIGINALE", "Originale : le jeu déterminera automatiquement la résolution des ombres." } }
        }
    },
    {
        ELanguage::Spanish,
        {
            { EShadowResolution::Original, { "ORIGINAL", "Original: el juego determinará de forma automática la resolución de las sombras." } }
        }
    },
    {
        ELanguage::Italian,
        {
            { EShadowResolution::Original, { "ORIGINALE", "Originale: il gioco determinerà automaticamente la risoluzione delle ombre." } }
        }
    }
};

// Japanese Notes: This localization should include furigana.
CONFIG_DEFINE_LOCALE(GITextureFiltering)
{
    { ELanguage::English,  { "GI Texture Filtering", "Change the quality of the filtering used for global illumination textures." } },
    { ELanguage::Japanese, { "GIテクスチャフィルタリング", "GIテクスチャに\u200B[使用:しよう]する\u200Bフィルタリングの\u200B[品質:ひんしつ]を\u200B[変更:へんこう]できます" } },
    { ELanguage::German,   { "GI Texturfilterung", "Ändere die Qualität der Filterung für Global-Illumination-Texturen." } },
    { ELanguage::French,   { "Filtre des textures IG", "Modifie la qualité du filtre utilisé pour les textures d'illumination globale." } },
    { ELanguage::Spanish,  { "Filtrado de texturas de GI", "Cambia la calidad del filtrado utilizado en las texturas de iluminación global." } },
    { ELanguage::Italian,  { "Filtraggio delle texture GI", "Regola la qualità del sistema di filtraggio utilizzato per le texture dell'illuminazione." } }
};

CONFIG_DEFINE_ENUM_LOCALE(EGITextureFiltering)
{
    {
        ELanguage::English,
        {
            { EGITextureFiltering::Bilinear, { "BILINEAR", "" } },
            { EGITextureFiltering::Bicubic,  { "BICUBIC", "" } }
        }
    },
    {
        ELanguage::Japanese,
        {
            { EGITextureFiltering::Bilinear, { "バイリニア", "" } },
            { EGITextureFiltering::Bicubic,  { "バイキュービック", "" } },
        }
    },
    {
        ELanguage::German,
        {
            { EGITextureFiltering::Bilinear, { "BILINEAR", "" } },
            { EGITextureFiltering::Bicubic,  { "BICUBIC", "" } }
        }
    },
    {
        ELanguage::French,
        {
            { EGITextureFiltering::Bilinear, { "BILINÉAIRE", "" } },
            { EGITextureFiltering::Bicubic,  { "BICUBIQUE", "" } },
        }
    },
    {
        ELanguage::Spanish,
        {
            { EGITextureFiltering::Bilinear, { "BILINEAR", "" } },
            { EGITextureFiltering::Bicubic,  { "BICÚBICO", "" } },
        }
    },
    {
        ELanguage::Italian,
        {
            { EGITextureFiltering::Bilinear, { "BILINEARE", "" } },
            { EGITextureFiltering::Bicubic,  { "BICUBICO", "" } }
        }
    }
};

// Japanese Notes: This localization should include furigana.
CONFIG_DEFINE_LOCALE(MotionBlur)
{
    { ELanguage::English,  { "Motion Blur", "Change the quality of the motion blur." } },
    { ELanguage::Japanese, { "モーションブラー", "モーションブラーの\u200B[品質:ひんしつ]を\u200B[変更:へんこう]できます" } },
    { ELanguage::German,   { "Bewegungsunschärfe", "Verändere die Qualität der Bewegungsunschärfe." } },
    { ELanguage::French,   { "Flou de mouvement", "Modifie la qualité du flou de mouvement." } },
    { ELanguage::Spanish,  { "Desenfoque de movimiento", "Cambia la calidad del desenfoque de movimiento." } },
    { ELanguage::Italian,  { "Sfocatura di movimento", "Regola la qualità della sfocatura di movimento." } }
};

// Japanese Notes: This localization should include furigana in its description.
CONFIG_DEFINE_ENUM_LOCALE(EMotionBlur)
{
    {
        ELanguage::English,
        {
            { EMotionBlur::Off,      { "OFF", "" } },
            { EMotionBlur::Original, { "ORIGINAL", "" } },
            { EMotionBlur::Enhanced, { "ENHANCED", "Enhanced: uses more samples for smoother motion blur at the cost of performance." } }
        }
    },
    {
        ELanguage::Japanese,
        {
            { EMotionBlur::Off,      { "オフ", "" } },
            { EMotionBlur::Original, { "オリジナル", "" } },
            { EMotionBlur::Enhanced, { "エンハンスド", "エンハンスド: パフォーマンスを\u200B[犠牲:ぎせい]にして\u200Bより[多:おお]くの\u200Bサンプルを\u200B[使用:しよう]してより\u200B[滑:なめ]らかな\u200Bモーションブラーを\u200B[実現:じつげん]します" } }
        }
    },
    {
        ELanguage::German,
        {
            { EMotionBlur::Off,      { "AUS", "" } },
            { EMotionBlur::Original, { "ORIGINAL", "" } },
            { EMotionBlur::Enhanced, { "VERBESSERT", "Verbessert: Benutzt mehr Samples um eine weichere Bewegungsunschärfe auf Kosten von Performanz zu erzeugen." } }
        }
    },
    {
        ELanguage::French,
        {
            { EMotionBlur::Off,      { "AUCUN", "" } },
            { EMotionBlur::Original, { "ORIGINAL", "" } },
            { EMotionBlur::Enhanced, { "AMÉLIORÉ", "Amélioré : utilise plus d'échantillons pour un flou de mouvement plus lisse au détriment des performances." } }
        }
    },
    {
        ELanguage::Spanish,
        {
            { EMotionBlur::Off,      { "APAGADO", "" } },
            { EMotionBlur::Original, { "ORIGINAL", "" } },
            { EMotionBlur::Enhanced, { "MEJORADO", "Mejorado: utiliza más muestras para suavizar el desenfoque de movimiento a costa del rendimiento." } }
        }
    },
    {
        ELanguage::Italian,
        {
            { EMotionBlur::Off,      { "DISATTIVATO", "" } },
            { EMotionBlur::Original, { "ORIGINALE", "" } },
            { EMotionBlur::Enhanced, { "AUMENTATO", "Aumentato: usa più passaggi per una sfocatura più fluida, con un impatto sulle prestazioni." } }
        }
    }
};

// Japanese Notes: This localization should include furigana.
CONFIG_DEFINE_LOCALE(XboxColorCorrection)
{
    { ELanguage::English,  { "Xbox Color Correction", "Use the warm tint from the Xbox version of the game." } },
    { ELanguage::Japanese, { "Xboxの[色補正:いろほせい]", "Xbox[版:ばん]のゲームの\u200B[暖色系:だんしょくけい]の[色合:いろあ]いを\u200B[使用:しよう]します" } },
    { ELanguage::German,   { "Xbox Farbkorrektur", "Benutzt den warmen Farbstich aus der Xbox-Version des Spiels." } },
    { ELanguage::French,   { "Correction couleurs Xbox", "Utilise le filtre de couleur provenant de la version Xbox du jeu." } },
    { ELanguage::Spanish,  { "Corrección de color de Xbox", "Utiliza el tono cálido de la versión Xbox del juego." } },
    { ELanguage::Italian,  { "Correzione dei colori Xbox", "Applica il filtro di colori più caldi utilizzato dalla versione Xbox del gioco." } }
};

// Japanese Notes: This localization should include furigana.
CONFIG_DEFINE_LOCALE(CutsceneAspectRatio)
{
    { ELanguage::English,  { "Cutscene Aspect Ratio", "Change the aspect ratio of the real-time cutscenes." } },
    { ELanguage::Japanese, { "アスペクト[比:ひ]のカットシーン", "リアルタイム\u200Bカットシーンの\u200Bアスペクト[比:ひ]を\u200B[変更:へんこう]できます" } },
    { ELanguage::German,   { "Zwischensequenz-Seitenverhältnis", "Verändere das Seitenverhältnis der Echtzeit-Zwischensequenzen." } },
    { ELanguage::French,   { "Format des cinématiques", "Modifie le format d'image des cinématiques en temps réel." } },
    { ELanguage::Spanish,  { "Relación de aspecto de cinemáticas", "Cambia la relación de aspecto de las cinemáticas de tiempo real." } },
    { ELanguage::Italian,  { "Rapporto d'aspetto dei filmati", "Cambia il rapporto d'aspetto dei filmati in tempo reale." } }
};

// Japanese Notes: This localization should include furigana in its description.
CONFIG_DEFINE_ENUM_LOCALE(ECutsceneAspectRatio)
{
    {
        ELanguage::English,
        {
            { ECutsceneAspectRatio::Original, { "ORIGINAL", "Original: locks cutscenes to their original 16:9 aspect ratio." } },
            { ECutsceneAspectRatio::Unlocked, { "UNLOCKED", "Unlocked: allows cutscenes to adjust their aspect ratio to the window size.\n\nWARNING: this will introduce visual oddities past the original 16:9 aspect ratio." } }
        }
    },
    {
        ELanguage::Japanese,
        {
            { ECutsceneAspectRatio::Original, { "オリジナル", "オリジナル: カットシーンを\u200B[元:もと]の\u200B16:9の\u200Bアスペクト[比:ひ]に\u200B[固定:こてい]します" } },
            { ECutsceneAspectRatio::Unlocked, { "解除", "[解除:かいじょ]: カットシーンの\u200Bアスペクト[比:ひ]を\u200Bウィンドウサイズに\u200B[合:あ]わせて\u200B[調整:ちょうせい]します\n\n[警告:けいこく]: [元:もと]の\u200B16:9の\u200Bアスペクト[比:ひ]を\u200B[超:こ]えると\u200B[視覚的:しかくてき]な\u200B[異常:いじょう]が\u200B[発生:はっせい]します" } },
        }
    },
    {
        ELanguage::German,
        {
            { ECutsceneAspectRatio::Original, { "ORIGINAL", "Original: Behält die Zwischensequenzen in ihrem originalen 16:9 Seitenverhältnis." } },
            { ECutsceneAspectRatio::Unlocked, { "ENTSPERRT", "Entsperrt: Erlaubt Zwischensequenzen ihr Seitenverhältnis der Fenstergröße anzupassen.\n\nWARNUNG: Diese Option kann zu visuellen Fehlern außerhalb der Grenzen des ursprünglichen Seitenverhältnisses führen." } }
        }
    },
    {
        ELanguage::French,
        {
            { ECutsceneAspectRatio::Original, { "ORIGINAL", "Original : force les cinématiques dans leur format 16:9 d'origine." } },
            { ECutsceneAspectRatio::Unlocked, { "LIBRE", "Libre : permet aux cinématiques d'adapter leur format d'image à la taille de la fenêtre.\n\nATTENTION : au delà du format 16:9 d'origine, des bugs visuels apparaitront." } },
        }
    },
    {
        ELanguage::Spanish,
        {
            { ECutsceneAspectRatio::Original, { "ORIGINAL", "Original: muestra las cinemáticas en su relación de aspecto original de 16:9." } },
            { ECutsceneAspectRatio::Unlocked, { "DESBLOQUEADO", "Desbloqueado: permite que las cinemáticas ajusten su relación de aspecto al tamaño de la ventana.\n\nADVERTENCIA: esto introducirá rarezas visuales más allá de la relación de aspecto original de 16:9." } },
        }
    },
    {
        ELanguage::Italian,
        {
            { ECutsceneAspectRatio::Original, { "ORIGINALE", "Originale: blocca il rapporto d'aspetto dei filmati a 16:9." } },
            { ECutsceneAspectRatio::Unlocked, { "SBLOCCATO", "Sbloccato: il rapporto d'aspetto verrà regolato in base alle dimensioni della finestra.\n\nATTENZIONE: questa opzione potrebbe causare dei problemi visivi se il rapporto d'aspetto è oltre 16:9." } }
        }
    }
};

// Japanese Notes: This localization should include furigana.
CONFIG_DEFINE_LOCALE(UIAlignmentMode)
{
    { ELanguage::English,  { "UI Alignment Mode", "Change how the UI aligns with the display." } },
    { ELanguage::Japanese, { "UIアライメントモード", "UIとディスプレイの\u200B[配置:はいち]を\u200B[変更:へんこう]できます" } },
    { ELanguage::German,   { "Benutzeroberflächenausrichtung", "Verändere wie die Benutzeroberfläche sich mit dem Bildschirm ausrichtet." } },
    { ELanguage::French,   { "Alignement de l'IU", "Modifie l'alignement de l'interface utilisateur sur l'écran." } },
    { ELanguage::Spanish,  { "Modo de alineamiento de UI", "Cambia la alineación de la interfaz de usuario con la pantalla." } },
    { ELanguage::Italian,  { "Modalità allineamento interfaccia", "Modifica come l'interfaccia si allinea con lo schermo." } }
};

// Japanese Notes: This localization should include furigana in its description.
CONFIG_DEFINE_ENUM_LOCALE(EUIAlignmentMode)
{
    {
        ELanguage::English,
        {
            { EUIAlignmentMode::Edge,    { "EDGE", "Edge: the UI will align with the edges of the display." } },
            { EUIAlignmentMode::Centre,  { "CENTER", "Center: the UI will align with the center of the display." } }
        }
    },
    {
        ELanguage::Japanese,
        {
            { EUIAlignmentMode::Edge,    { "エッジ", "エッジ: UIがディスプレイの\u200B[端:はし]に\u200B[揃:そろ]います" } },
            { EUIAlignmentMode::Centre,  { "センター", "センター: UIがディスプレイの\u200B[中央:ちゅうおう]に\u200B[揃:そろ]います" } },
        }
    },
    {
        ELanguage::German,
        {
            { EUIAlignmentMode::Edge,    { "RAND", "Rand: Die Benutzeroberfläche richtet sich zum Rand des Bildschirms aus." } },
            { EUIAlignmentMode::Centre,  { "MITTE", "Mitte: Die Benutzeroberfläche richtet sich zur Mitte des Bildschirms aus." } }
        }
    },
    {
        ELanguage::French,
        {
            { EUIAlignmentMode::Edge,    { "BORD", "Bord : l'interface utilisateur sera alignée sur les bords de l'écran." } },
            { EUIAlignmentMode::Centre,  { "CENTRÉE", "Centrée : l'interface utilisateur sera alignée sur le centre de l'écran." } },
        }
    },
    {
        ELanguage::Spanish,
        {
            { EUIAlignmentMode::Edge,    { "BORDE", "Borde: la interfaz de usuario se alineará con los bordes de la pantalla." } },
            { EUIAlignmentMode::Centre,  { "CENTRO", "Centro: la interfaz de usuario se alineará con el centro de la pantalla." } },
        }
    },
    {
        ELanguage::Italian,
        {
            { EUIAlignmentMode::Edge,    { "BORDI", "Bordi: l'interfaccia si allineerà con i bordi dello schermo." } },
            { EUIAlignmentMode::Centre,  { "CENTRO", "Centro: l'interfaccia si allineerà con il centro dello schermo." } }
        }
    }
};
