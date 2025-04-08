#include <WiFi.h>
#include <WebServer.h>
#include <EEPROM.h>
#include <SPIFFS.h>
#include <FS.h>
// #include <ArduinoJson.h>
#include "mbedtls/aes.h"
#include <mbedtls/base64.h>
#include <mbedtls/md.h>
#include <mbedtls/sha256.h>

// Tamaños definidos
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
// Onboard LED
#define LED_PIN 2  
//Variables a ser recuperadas de la EEPROM
uint8_t aesKey[AES_KEY_SIZE];
uint8_t iv[IV_SIZE];
uint8_t jwtKey[JWT_KEY_SIZE];
uint8_t encryptedUser[ENCRYPTED_USER_SIZE];
uint8_t encryptedPass[ENCRYPTED_PASS_SIZE];
//Leer credenciales en hardware
String retrieveData(uint8_t* element, int len){
  String data = "";
  for (int i = 0; i < len; i++) {
    data += (char)element[i];
  }
  return data;
}
//Leer data de la EEPROM
void readEEPROMData() {
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
void printByteArray(uint8_t* arr, int len) {
  for (int i = 0; i < len; i++) {
    if (arr[i] < 16) Serial.print("0");
    Serial.print(arr[i], HEX);
    Serial.print(" ");
  }
  Serial.println();
}
//AES Function
void decryptAES(const uint8_t *input, uint8_t *output, size_t length) {
  mbedtls_aes_context aes;
  mbedtls_aes_init(&aes);
  mbedtls_aes_setkey_enc(&aes, aesKey, 128);
  // copia mutable del IV
  uint8_t iv_copy[16];  
  memcpy(iv_copy, iv, 16);  
  mbedtls_aes_crypt_cbc(&aes, MBEDTLS_AES_DECRYPT, length, iv_copy, input, output);
  mbedtls_aes_free(&aes);
}
//Credenciales temporales
char correctUser[] = "admin";
char correctPass[] = "abc123";

// Initialize the server library
// (port 80 is default for HTTP):
WiFiServer server(80);

// const char* ssid = "";
// const char* password = "";
const char* ssid = "TECNO SPARK 20";
const char* password = "u4dvfrq9yquaydm";
// const char* ssid = ".TigoWiFi-307776204/0";
// const char* password = "WiFi-94330935";
// const char* ssid = ".TigoWiFi-253467004/0";
// const char* password = "WiFi-89775044";

int failedAttempts = 0;
unsigned long lockoutTime = 0;
String token = "";
// Clave secreta para firmar el JWT (debe ser segura en producción)
const char* secretKey = "my_super_secret_key";
unsigned long jwtExpTime = 0;
// Función para generar JWT
String createJWT(const char* user) {
  // Crear el header en formato JSON
  StaticJsonDocument<200> header;
  header["alg"] = "HS256";
  header["typ"] = "JWT";
  
  // Serializar el header a string
  String headerStr;
  serializeJson(header, headerStr);
  
  // Codificar el header en Base64
  char headerBase64[256];
  size_t headerLen;
  mbedtls_base64_encode((unsigned char*)headerBase64, sizeof(headerBase64), &headerLen, (unsigned char*)headerStr.c_str(), headerStr.length());
  headerBase64[headerLen] = '\0';

  // Crear el payload
  StaticJsonDocument<200> payload;
  payload["sub"] = user;
  payload["iat"] = millis();
  payload["exp"] = jwtExpTime;

  // Serializar el payload a string
  String payloadStr;
  serializeJson(payload, payloadStr);

  // Codificar el payload en Base64
  char payloadBase64[256];
  size_t payloadLen;
  mbedtls_base64_encode((unsigned char*)payloadBase64, sizeof(payloadBase64), &payloadLen, (unsigned char*)payloadStr.c_str(), payloadStr.length());
  payloadBase64[payloadLen] = '\0';

  // Crear la firma usando HMAC-SHA256
  String signingInput = String(headerBase64) + "." + String(payloadBase64);
  unsigned char hmacResult[32];
  mbedtls_md_hmac(mbedtls_md_info_from_type(MBEDTLS_MD_SHA256), (unsigned char*)secretKey, strlen(secretKey), (unsigned char*)signingInput.c_str(), signingInput.length(), hmacResult);

  // Codificar la firma en Base64
  char signatureBase64[256];
  size_t signatureLen;
  mbedtls_base64_encode((unsigned char*)signatureBase64, sizeof(signatureBase64), &signatureLen, hmacResult, sizeof(hmacResult));
  signatureBase64[signatureLen] = '\0';

  // Construir el JWT final
  String jwt = String(headerBase64) + "." + String(payloadBase64) + "." + String(signatureBase64);
  return jwt;
}
//Funcion para sanitizar entradas de usuario (Solo permite caracteres alfanuméricos)
bool isValidInput(String input) {
  for (unsigned int i = 0; i < input.length(); i++) {
      if (!isalnum(input[i])) return false;
  }
  return true;
}
//Funcion para inicializar WiFi
void initWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  WiFi.setAutoReconnect(true); //Para conexion inestable
  WiFi.persistent(true);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nConectado a WiFi.");
  Serial.print("Dirección IP: ");
  Serial.println(WiFi.localIP());
  server.begin(); // Inicia el servidor
}

void setup() 
{
  Serial.begin(115200);// Open serial communications and wait for port to open:
  // Initialize SPIFFS
  if(!SPIFFS.begin(true)){
    Serial.println("An Error has occurred while mounting SPIFFS");
    return;
  }
  delay(1000);
  initWiFi();
  
  pinMode(LED_PIN,OUTPUT); // Pin digital 13 como salida
}

void loop() 
{
  WiFiClient client = server.available();
  
  // Maneja las solicitudes HTTP entrantes
  String headers = "";
  String body = "";
  bool isBody = false;
  if (client) {   
    delay(50);
    if (client.connected()) {
      //Serial.println("Client connected");
      while (client.available()) {
        char c = client.read();
        // Leer encabezados hasta encontrar la línea en blanco
        if (!isBody) {
            headers += c;
            if (headers.endsWith("\r\n\r\n")) {
                isBody = true; // La siguiente parte será el cuerpo
            }
        } else {
            // Leer el cuerpo del request
            body += c;
        }
      }   
      int tokenPos = headers.indexOf("token=");
      String retrivedToken = "";
      if(tokenPos != -1) {
        int initPos = 0;
        while(true){
          if (initPos == 0) {
            initPos = tokenPos + 6;
          }
          char x = headers.charAt(initPos);
          if(x=='\n'){
            break;
          }
          retrivedToken += x;
          initPos += 1;
        }
      }
      token.trim();
      retrivedToken.trim();
      // Serial.println("Headers:");
      // Serial.println(headers);
      // Serial.println("Body");
      // Serial.println(body);
      if (token.length()>0&&token.equals(retrivedToken)) {handleLED(headers);}
      //Resources Handlers
      if (headers.indexOf("GET /index.js") != -1) {serveResource(client, "/index.js", "application/javascript");}
      if (headers.indexOf("GET /login.css") != -1) {serveResource(client, "/login.css", "text/css");}
      if (headers.indexOf("GET /index.css") != -1) {serveResource(client, "/index.css", "text/css");}
      if (headers.indexOf("GET /password_change.css") != -1) {serveResource(client, "/password_change.css", "text/css");}
      if (headers.indexOf("GET /logo.png") != -1) {serveResource(client,"/logo.png", "image/png");}
      delay(100); //Allow time to load resources before pages
      if (headers.indexOf("POST /login HTTP/1.1") != -1) {
        handleLogin(client,body);
      } else if (token.length()>0&&token.equals(retrivedToken)) {
        //Authentication required content goes down
        if(headers.indexOf("GET /led") != -1) {
          serveResource(client,"/index.html", "text/html");
          //keepAlive(headers);
        } else if (headers.indexOf("GET /logout HTTP/1.1") != -1) {
          Serial.println("Logout");
          token = "";
          client.println("HTTP/1.1 302 Found"); 
          client.println("Location: / ");  // Redirigir a inicio
          client.println("Set-Cookie: token=; Path=/; HttpOnly; SameSite=Strict");
          client.println("Content-Type: text/html");
          client.println(); 
        } else if (headers.indexOf("GET /change_password HTTP/1.1") != -1) {
          serveResource(client,"/password_change.html", "text/html");
        } else if (headers.indexOf("POST /cpassword") != -1) {
          Serial.println("Requested to change password");
          client.println("HTTP/1.1 302 Found"); 
          client.println("Location: /led");  // Redirigir a led
          client.println("Content-Type: text/html");
          client.println(); 
        }
      } else {
        serveResource(client,"/login.html", "text/html");
      }
    }
    delay(50); 
    client.flush(); 
    client.stop();
    //Serial.println("Client disconnected");
  } 
}
// Process login
void handleLogin(WiFiClient client, String request) {
  if (failedAttempts >= 5 && millis() - lockoutTime < 60000) {  // 60 segundos de bloqueo
    client.println("HTTP/1.1 429 Too Many Requests");
    client.println("Retry-After: 60");
    client.println("Content-Type: text/plain");
    client.println();
    client.println("Demasiados intentos fallidos. Intente nuevamente en 60 segundos.");
    return;
  }
  int userPos = request.indexOf("user=");
  int passPos = request.indexOf("pass=");

  if (userPos == -1 || passPos == -1) {
    Serial.println("Error: No se encontraron parámetros 'user' y 'pass'.");
    client.println("HTTP/1.1 400 Bad Request\r\nContent-Type: text/plain\r\n\r\nMissing credentials");
    return;
  }

  int userEnd = request.indexOf("&", userPos);
  int passEnd = request.indexOf(" ", passPos);

  if (userEnd == -1) userEnd = passPos - 1;
  if (passEnd == -1) passEnd = request.length();

  String user = request.substring(userPos + 5, userEnd);
  String pass = request.substring(passPos + 5, passEnd);

  user.trim();
  pass.trim();

  if (!isValidInput(user) || !isValidInput(pass)) {
    Serial.println("Error: User y Password deben ser alfanumericos.");
    client.println("HTTP/1.1 400 Bad Request\r\nContent-Type: text/plain\r\n\r\nBad credentials");
    return;
  }

  if (user.equals(correctUser) && pass.equals(correctPass)) {
    jwtExpTime = millis() + 60;
    token = createJWT(user.c_str());
    failedAttempts = 0;
    Serial.println("Login exitoso!");
    client.println("HTTP/1.1 302 Found");  // Redirección HTTP
    client.println("Location: /led");  // Redirigir a /led
    client.println("Set-Cookie: token=" + token + "; Path=/; HttpOnly; SameSite=Strict");
    client.println("Content-Type: text/html");
    client.println();
  } else {
    failedAttempts++;
    if (failedAttempts >= 5) {
      lockoutTime = millis();
    }
    Serial.println("Login fallido: usuario o contraseña incorrectos.");
    client.println("HTTP/1.1 401 Unauthorized\r\nContent-Type: text/plain\r\n\r\nInvalid credentials");
    client.println("Connection: close");
  }
}
//Control LED
void handleLED(String request) {
  if (request.indexOf("led_status=on") != -1) {
      digitalWrite(LED_PIN, HIGH);
      Serial.println("LED encendido");
  } else if (request.indexOf("led_status=off") != -1) {
      digitalWrite(LED_PIN, LOW);
      Serial.println("LED apagado");
  }
}
//Keep session alive
void keepAlive(String request) {
  if (request.indexOf("reset=true") != -1) {
    jwtExpTime = millis() + 60;
  }
}
//Serve web resources
void serveResource(WiFiClient& client, const char* path, String resourceType) {
  File file = SPIFFS.open(path, "r");
  if (!file) {
    Serial.println("Archivo no encontrado");
    client.println("HTTP/1.1 404 Not Found");
    client.println("Connection: close");
    client.println();
    return;
  }

  client.println("HTTP/1.1 200 OK");
  client.println("Content-Type: " + resourceType);
  if(resourceType!="text/html"){client.println("Cache-Control: max-age=86400, public");} //cache all resources that aren't webpages
  client.println("Connection: close");
  client.println();

  while (file.available()) {
    client.write(file.read());
  }
  file.close();
}