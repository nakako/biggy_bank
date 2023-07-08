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

/* 関数宣言 */
void setup(void);
uint8_t level_get(void);
void output_GPIO_level1(void);
void output_GPIO_level2(void);
void output_GPIO_level3(void);
void send_level_to_M5(uint8_t level);
void loop(void);


void setup() {
  char apn[LTE_NET_APN_MAXLEN] = APP_LTE_APN;
  LTENetworkAuthType authtype = APP_LTE_AUTH_TYPE;
  char user_name[LTE_NET_USER_MAXLEN] = APP_LTE_USER_NAME;
  char password[LTE_NET_PASSWORD_MAXLEN] = APP_LTE_PASSWORD;

  /* M5への出力用初期設定 */
  // M5への出力用ポートのモード設定
  pinMode(31, OUTPUT);  //Level.3用
  pinMode(32, OUTPUT);  //Level.2用
  pinMode(07, OUTPUT);  //Level.1用
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

void send_level_to_M5(uint8_t level) {
  switch (level) {
    case 1:
      output_GPIO_level1();
      break;
    case 2:
      output_GPIO_level2();
      break;
    case 3:
      output_GPIO_level3();
      break;
    default:
      printf("level error : %d", level);
  }
  return;
}

void loop() {

  uint8_t new_level = level_get();

  // Levelに変化があった場合
  if (new_level != 0) {
    // データ有り
    if (old_level != new_level) {
      printf("Levelが変化しました : %d -> %d\n", old_level, new_level);

      /* LevelをM5に送る */
      printf("Level %d をM5に送信\n\n", new_level);
      send_level_to_M5(new_level);

      /* 音を鳴らす */

      //Levelの更新
      old_level = new_level;

    } else {
      printf("Level変化なし\n");
    }
  } else {
    // データなし
    printf("データなし\n\n");
  }
  sleep(1);
}
