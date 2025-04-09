#include <EEPROM.h>
#include <mbedtls/sha256.h>
#include "mbedtls/aes.h"

// Tamaños definidos
#define EEPROM_SIZE          128
#define AES_KEY_SIZE         16
#define IV_SIZE              16
#define JWT_KEY_SIZE         32
#define ENCRYPTED_USER_SIZE  16
#define ENCRYPTED_PASS_SIZE  16
// Direcciones base
#define ADDR_AES_KEY         0
#define ADDR_IV              (ADDR_AES_KEY + AES_KEY_SIZE)
#define ADDR_JWT_KEY         (ADDR_IV + IV_SIZE)
#define ADDR_ENC_USER        (ADDR_JWT_KEY + JWT_KEY_SIZE)
#define ADDR_ENC_PASS        (ADDR_ENC_USER + ENCRYPTED_USER_SIZE)

String inputUser, inputPass, inputJwt;

void setup() {
  Serial.begin(115200);
  EEPROM.begin(EEPROM_SIZE);
  delay(1000);
  Serial.println("=== Configuracion de EEPROM ===");
  
  // Solicitar datos por puerto serial
  getUserInput("Ingrese el usuario (max 16 chars):", inputUser);
  getUserInput("Ingrese la contraseña (max 16 chars):", inputPass);
  getUserInput("Ingrese la clave JWT (será convertida a SHA-256 simulada):", inputJwt);

  writeEEPROMData(inputUser, inputPass, inputJwt);
  delay(1000);
  readEEPROMData();
}

void loop() {
  // Nada en el loop
}

void getUserInput(String message, String &input) {
  Serial.println(message);
  while (input.length() == 0) {
    if (Serial.available()) {
      input = Serial.readStringUntil('\n');
      input.trim();
    }
  }
}

// hash a 32 bytes
void sha256Hash(const String &input, uint8_t* output) {
  // Convertir String de Arduino a array de bytes
  const char* inputStr = input.c_str();
  size_t inputLen = strlen(inputStr);

  // Calcular hash SHA-256
  mbedtls_sha256_context ctx;
  mbedtls_sha256_init(&ctx);
  mbedtls_sha256_starts(&ctx, 0); // 0 = SHA-256 (no SHA-224)
  mbedtls_sha256_update(&ctx, (const unsigned char*)inputStr, inputLen);
  mbedtls_sha256_finish(&ctx, output);
  mbedtls_sha256_free(&ctx);
}

void writeEEPROMData(String user, String pass, String jwt) {
  // Claves estáticas (puedes cambiarlas)
  uint8_t aesKey[AES_KEY_SIZE] = { 
    0xff, 0xcc, 0xcf, 0x3e, 0x0d, 
    0xe0, 0x3d, 0xf1, 0x5a, 0xb8, 
    0x39, 0xdc, 0x72, 0x79, 0xeb, 0x3f 
  }; 

  uint8_t iv[IV_SIZE] = { 
    0xa9, 0x31, 0x62, 0x8d, 
    0x2a, 0x26, 0x4f, 0x0f, 
    0x01, 0xa3, 0x6e, 0x63, 
    0x38, 0xb5, 0xc9, 0xb2 
  };

  uint8_t jwtKey[JWT_KEY_SIZE];
  sha256Hash(jwt, jwtKey);

  uint8_t encryptedUser[ENCRYPTED_USER_SIZE] = {0};
  uint8_t encryptedPass[ENCRYPTED_PASS_SIZE] = {0};

  // Copiar strings al buffer limitado
  for (int i = 0; i < ENCRYPTED_USER_SIZE && i < user.length(); i++) {
    encryptedUser[i] = user[i];
  }
  for (int i = 0; i < ENCRYPTED_PASS_SIZE && i < pass.length(); i++) {
    encryptedPass[i] = pass[i];
  }

  // Escritura
  for (int i = 0; i < AES_KEY_SIZE; i++) EEPROM.write(ADDR_AES_KEY + i, aesKey[i]);
  for (int i = 0; i < IV_SIZE; i++) EEPROM.write(ADDR_IV + i, iv[i]);
  for (int i = 0; i < JWT_KEY_SIZE; i++) EEPROM.write(ADDR_JWT_KEY + i, jwtKey[i]);
  for (int i = 0; i < ENCRYPTED_USER_SIZE; i++) EEPROM.write(ADDR_ENC_USER + i, encryptedUser[i]);
  for (int i = 0; i < ENCRYPTED_PASS_SIZE; i++) EEPROM.write(ADDR_ENC_PASS + i, encryptedPass[i]);

  EEPROM.commit();
  Serial.println("✅ Datos escritos en EEPROM.");
}

void readEEPROMData() {
  uint8_t aesKey[AES_KEY_SIZE];
  uint8_t iv[IV_SIZE];
  uint8_t jwtKey[JWT_KEY_SIZE];
  uint8_t encryptedUser[ENCRYPTED_USER_SIZE];
  uint8_t encryptedPass[ENCRYPTED_PASS_SIZE];

  for (int i = 0; i < AES_KEY_SIZE; i++) aesKey[i] = EEPROM.read(ADDR_AES_KEY + i);
  for (int i = 0; i < IV_SIZE; i++) iv[i] = EEPROM.read(ADDR_IV + i);
  for (int i = 0; i < JWT_KEY_SIZE; i++) jwtKey[i] = EEPROM.read(ADDR_JWT_KEY + i);
  for (int i = 0; i < ENCRYPTED_USER_SIZE; i++) encryptedUser[i] = EEPROM.read(ADDR_ENC_USER + i);
  for (int i = 0; i < ENCRYPTED_PASS_SIZE; i++) encryptedPass[i] = EEPROM.read(ADDR_ENC_PASS + i);

  Serial.println("\n=== Datos leídos desde EEPROM ===");
  Serial.println("AES Key:");
  printByteArray(aesKey, AES_KEY_SIZE);

  Serial.println("IV:");
  printByteArray(iv, IV_SIZE);

  Serial.println("JWT Key (simulada):");
  printByteArray(jwtKey, JWT_KEY_SIZE);

  Serial.println("Usuario:");
  printTextOrHex(encryptedUser, ENCRYPTED_USER_SIZE);

  Serial.println("Contraseña:");
  printTextOrHex(encryptedPass, ENCRYPTED_PASS_SIZE);
}

void printByteArray(uint8_t* arr, int len) {
  for (int i = 0; i < len; i++) {
    if (arr[i] < 16) Serial.print("0");
    Serial.print(arr[i], HEX);
    Serial.print(" ");
  }
  Serial.println();
}

void printTextOrHex(uint8_t* arr, int len) {
  for (int i = 0; i < len; i++) {
    if (isPrintable(arr[i])) {
      Serial.print((char)arr[i]);
    } else {
      Serial.print(".");
    }
  }
  Serial.println();
}