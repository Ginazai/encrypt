#include <EEPROM.h>
#include <Arduino.h>
#include "mbedtls/aes.h"
#include <mbedtls/sha256.h>

#define EEPROM_SIZE 128  // Definimos un tamaño de 128 bytes para la EEPROM
//Generado con script de python para mayor seguridad
const uint8_t AES_KEY[16] = { 
    0xff, 0xcc, 0xcf, 0x3e, 
    0x0d, 0xe0, 0x3d, 0xf1, 
    0x5a, 0xb8, 0x39, 0xdc, 
    0x72, 0x79, 0xeb, 0x3f 
};
const uint8_t IV[16] = { 
    0xa9, 0x31, 0x62, 0x8d, 
    0x2a, 0x26, 0x4f, 0x0f, 
    0x01, 0xa3, 0x6e, 0x63, 
    0x38, 0xb5, 0xc9, 0xb2 
};
const char jwtK[] = {"HTKM86239676"}; 

void applyHash(const char *input, uint8_t *outputHash) {
  mbedtls_sha256_context ctx;
  mbedtls_sha256_init(&ctx);
  mbedtls_sha256_starts(&ctx, 0);  // 0 = SHA-256 normal, 1 = SHA-224
  mbedtls_sha256_update(&ctx, (const uint8_t*)input, strlen(input));
  mbedtls_sha256_finish(&ctx, outputHash);
  mbedtls_sha256_free(&ctx);

  Serial.print("SHA-256: ");
  for (int i = 0; i < 32; i++) {
    Serial.printf("%02X", outputHash[i]);
  }
  Serial.println();
}
void encryptAES(const uint8_t *input, uint8_t *output, size_t length) {
    mbedtls_aes_context aes;
    mbedtls_aes_init(&aes);
    mbedtls_aes_setkey_enc(&aes, AES_KEY, 128);
    
    // copia mutable del IV
    uint8_t iv_copy[16];  
    memcpy(iv_copy, IV, 16);  

    mbedtls_aes_crypt_cbc(&aes, MBEDTLS_AES_ENCRYPT, length, iv_copy, input, output);
    
    mbedtls_aes_free(&aes);
}

void writeEncryptedData(int address, const uint8_t *data, size_t length) {
  for (size_t i = 0; i < length; i++) {
    EEPROM.write(address + i, data[i]);
  }
  EEPROM.commit();
}

void showContent(uint8_t* element, int len){
  String item = "";
  for (int i = 0; i < len; i++) {
    item += (char)element[i];
  }
  Serial.println(item);
}
void setup() {
  Serial.begin(115200);
  //EEPROM.begin(EEPROM_SIZE);

  uint8_t username[16] = "admin"; 
  uint8_t password[32] = "abc123";

  delay(1000);
  uint8_t jwtKey[32];
  applyHash(jwtK, jwtKey);
  delay(1000);
  uint8_t encUsername[16], encPassword[32];
  encryptAES(username, encUsername, 16);
  encryptAES(password, encPassword, 32);
  delay(1000);
  showContent(encUsername, 16);
  showContent(encPassword, 32);
  showContent(jwtKey,32);

  Serial.println("Output hash:");
  for (int i = 0; i < 32; i++) {
    Serial.printf("%02X", jwtKey[i]);
  }

  showContent(username,16);
  showContent(password,32);

  // writeEncryptedData(0, jwtKey, 32);         // Guardamos la clave JWT en EEPROM
  // writeEncryptedData(32, encUsername, 16);  // Guardamos el username cifrado
  // writeEncryptedData(48, encPassword, 32);  // Guardamos el password cifrado
}

void loop() {
}
