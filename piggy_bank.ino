// libraries
#include "SipfClient.h"
#include <String>
#include <LTE.h>
#include <Camera.h>
#include <SDHCI.h>
#include <Audio.h>

/* LTE関連 */
LTE lteAccess;
LTEClient client;
SipfClient sipfClient;

/* output Level(貯金額Level)関連 */
uint8_t old_level = 1;

/* 効果音関連 */
SDClass theSD;
AudioClass *theAudio;
// Level.1は効果音なし
File Leve2_music_file;
File Leve3_music_file;
bool music_ErrEnd = false;

/* 音声ファイルの名前定義 */
// Level.1は効果音なし
#define Level2_music "Sound.mp3"
#define Level3_music "Sound.mp3"


/* 関数宣言 */

// サーバ -> SPRESENES : Level取得関連
static uint8_t level_get(void);
// SPRESENES -> M5 : Level送信関連
static void send_level_to_M5(uint8_t level);
static void output_GPIO_level1(void);
static void output_GPIO_level2(void);
static void output_GPIO_level3(void);
// 効果音関連
static void start_player(void);
static void stop_player(void);
static void output_music(File music_file);
static void audio_attention_cb(const ErrorAttentionParam *atprm);
// 初期化
static void setup(void);
// メインループ
static void loop(void);


/* サーバ -> SPRESENES : Level取得関連 -------------------------------------- */
uint8_t level_get() {
  uint64_t utime = 0;
  uint64_t objDataLength = 0;
  SipfObjectDown objDown;
  uint8_t revel = 0;
  uint8_t end_array = 0;

  memset(&objDown, 0, sizeof(objDown));

  // モノプラからオブジェクト群を取得(戻り値は、受信したオブジェクト群のデータのbyte数)
  printf("sipfClient.downloadObjects\n");
  objDataLength = sipfClient.downloadObjects(utime, &objDown);

  printf("  down_request_result         0x%02x\n", objDown.down_request_result);
  printf("  timestamp_src               %llu\n", objDown.timestamp_src);
  printf("  timestamp_platfrom_from_src %llu\n", objDown.timestamp_platfrom_from_src);
  printf("  remains                     %u\n", objDown.down_request_result);

  // 取得したオブジェクト群のデータをダンプ
  printf("  objDataLength               %llu\n", objDataLength);
  for (int i = 0; i < objDataLength; i++) {
    printf("    %03d 0x%02X\n", i, objDown.objects_data[i]);
    end_array = i;
  }
  printf("\n");
  revel = objDown.objects_data[end_array];
  printf("return revel : %d\n", revel);
  return revel;
}

/* SPRESENES -> M5 : Level送信関連 ----------------------------------------- */
void send_level_to_M5(uint8_t level) {
  switch (level) {
    case 1:
      output_GPIO_level1();
      // Level.1は効果音なし
      break;
    case 2:
      output_GPIO_level2();
      output_music(Leve2_music_file);
      break;
    case 3:
      output_GPIO_level3();
      output_music(Leve3_music_file);
      break;
    default:
      printf("level error : %d", level);
  }
  return;
}

void output_GPIO_level1() {
  digitalWrite(31, LOW);
  digitalWrite(32, LOW);
  digitalWrite(07, HIGH);
  digitalWrite(LED3, LOW);  //デバッグ用
  digitalWrite(LED2, LOW);
  digitalWrite(LED1, HIGH);
  return;
}

void output_GPIO_level2() {
  digitalWrite(31, LOW);
  digitalWrite(32, HIGH);
  digitalWrite(07, LOW);
  digitalWrite(LED3, LOW);  //デバッグ用
  digitalWrite(LED2, HIGH);
  digitalWrite(LED1, LOW);
  return;
}

void output_GPIO_level3() {
  digitalWrite(31, HIGH);
  digitalWrite(32, LOW);
  digitalWrite(07, LOW);
  digitalWrite(LED3, HIGH);  //デバッグ用
  digitalWrite(LED2, LOW);
  digitalWrite(LED1, LOW);
  return;
}


/* 効果音関連 ------------------------------------------------------------- */
void start_player(void) {
  // start audio system
  theAudio = AudioClass::getInstance();
  theAudio->begin(audio_attention_cb);

  puts("initialization Audio Library");

  // クロックモードを設定： MP3再生のため、AS_CLKMODE_NORMAL を選択
  theAudio->setRenderingClockMode(AS_CLKMODE_NORMAL);
  // プレイヤーモードを設定(スピーカーヘッドフォン、拡張ボードのヘッドホン端子を用いたラインアウト)
  theAudio->setPlayerMode(AS_SETPLAYER_OUTPUTDEVICE_SPHP, AS_SP_DRV_MODE_LINEOUT);
  // プレイヤーの初期化
  err_t err = theAudio->initPlayer(AudioClass::Player0, AS_CODECTYPE_MP3, "/mnt/sd0/BIN", AS_SAMPLINGRATE_AUTO, AS_CHANNEL_STEREO);
  if (err != AUDIOLIB_ECODE_OK) {
    printf("Player0 initialize error\n");
    exit(1);
  }

  //音量を設定(-16.0 dB) */
  theAudio->setVolume(-160);

  /* MP3ファイルの読み出し */
  // SD用設定
  while (!theSD.begin()) {
    /* SDカードが認識されるまでループ */
    Serial.println("Insert SD card.");
  }

  // Level.1は効果音なし
  Leve2_music_file = theSD.open(Level2_music);
  if (!Leve2_music_file) {
    printf("File open error\n");
    exit(1);
  }
  printf("Open! 0x%08lx\n", (uint32_t)Leve2_music_file);

  Leve3_music_file = theSD.open(Level3_music);
  if (!Leve3_music_file) {
    printf("File open error\n");
    exit(1);
  }
  printf("Open! 0x%08lx\n", (uint32_t)Leve3_music_file);

  puts("Player設定完了!");
  return;
}

void stop_player(void) {
  theAudio->stopPlayer(AudioClass::Player0);
  Leve2_music_file.close();
  Leve2_music_file.close();
  theAudio->setReadyMode();
  theAudio->end();
  return;
}

void output_music(File music_file) {
  // 前回効果音のStop(鳴りっぱなしだったら止める)
  theAudio->stopPlayer(AudioClass::Player0);

  // MP3 ファイルの内容をデコーダーへ書き込み
  err_t err = theAudio->writeFrames(AudioClass::Player0, music_file);
  if ((err != AUDIOLIB_ECODE_OK) && (err != AUDIOLIB_ECODE_FILEEND)) {
    printf("File Read Error! =%d\n", err);
    return;
  }
  theAudio->startPlayer(AudioClass::Player0);
  return;
}

// Audio attention callback
static void audio_attention_cb(const ErrorAttentionParam *atprm) {
  puts("Attention!");

  if (atprm->error_code >= AS_ATTENTION_CODE_WARNING) {
    music_ErrEnd = true;
  }
  return;
}

/* 初期化 -----------------------------------------*/
void setup() {
  char apn[LTE_NET_APN_MAXLEN] = APP_LTE_APN;
  LTENetworkAuthType authtype = APP_LTE_AUTH_TYPE;
  char user_name[LTE_NET_USER_MAXLEN] = APP_LTE_USER_NAME;
  char password[LTE_NET_PASSWORD_MAXLEN] = APP_LTE_PASSWORD;

  /* Audio用設定 */
  start_player();

  /* M5への出力用初期設定 */
  // M5への出力用ポートのモード設定
  pinMode(31, OUTPUT);    //Level.3用
  pinMode(32, OUTPUT);    //Level.2用
  pinMode(07, OUTPUT);    //Level.1用
  pinMode(LED3, OUTPUT);  //デバッグ用
  pinMode(LED2, OUTPUT);
  pinMode(LED1, OUTPUT);
  // M5への出力(初期値：Level.1)
  output_GPIO_level1();


  // initialize serial communications and wait for port to open:
  Serial.begin(115200);
  while (!Serial) {
    ;  // wait for serial port to connect. Needed for native USB port only
  }

  // Chech Access Point Name is empty
  if (strlen(APP_LTE_APN) == 0) {
    Serial.println("ERROR: This sketch doesn't have a APN information.");
    while (1) {
      sleep(1);
    }
  }
  Serial.println("=========== APN information ===========");
  Serial.print("Access Point Name  : ");
  Serial.println(apn);
  Serial.print("Authentication Type: ");
  Serial.println(authtype == LTE_NET_AUTHTYPE_CHAP ? "CHAP" : authtype == LTE_NET_AUTHTYPE_NONE ? "NONE"
                                                                                                : "PAP");
  if (authtype != LTE_NET_AUTHTYPE_NONE) {
    Serial.print("User Name          : ");
    Serial.println(user_name);
    Serial.print("Password           : ");
    Serial.println(password);
  }

  while (true) {

    /* Power on the modem and Enable the radio function. */

    if (lteAccess.begin() != LTE_SEARCHING) {
      Serial.println("Could not transition to LTE_SEARCHING.");
      Serial.println("Please check the status of the LTE board.");
      for (;;) {
        sleep(1);
      }
    }

    /* The connection process to the APN will start.
         * If the synchronous parameter is false,
         * the return value will be returned when the connection process is started.
         */
    if (lteAccess.attach(APP_LTE_RAT,
                         apn,
                         user_name,
                         password,
                         authtype,
                         APP_LTE_IP_TYPE)
        == LTE_READY) {
      Serial.println("Attach succeeded.");
      break;
    }

    /* If the folLOWing logs occur frequently, one of the folLOWing might be a cause:
         * - APN settings are incorrect
         * - SIM is not inserted correctly
         * - If you have specified LTE_NET_RAT_NBIOT for APP_LTE_RAT,
         *   your LTE board may not support it.
         * - Rejected from LTE network
         */
    Serial.println("An error has occurred. Shutdown and retry the network attach process after 1 second.");
    lteAccess.shutdown();
    sleep(1);
  }

  sipfClient.begin(&client, 80);

  if (!sipfClient.authenticate()) {
    Serial.println("SIFP Authentication error");
  }
}

/* メインループ ------------------------------------*/
void loop() {

  uint8_t new_level = level_get();

  // Levelに変化があった場合
  if (new_level != 0) {
    // データ有り
    if (old_level != new_level) {
      printf("Levelが変化しました : %d -> %d\n", old_level, new_level);

      /* LevelをM5に送る, 効果音を鳴らす */
      printf("Level %d をM5に送信\n\n", new_level);
      send_level_to_M5(new_level);

      //Levelの更新
      old_level = new_level;

    } else {
      printf("Level変化なし\n");
    }
  } else {
    // データなし
    printf("データなし\n\n");
  }

  /* 効果音処理でエラーが出た時の処理 */
  // if (music_ErrEnd == true) {
  //   stop_player();
  //   start_player();
  // }

  sleep(1);
}
