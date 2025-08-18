// --- Bibliotecas ---
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <EEPROM.h>
#include <Keypad.h>
#include <Servo.h>

// --- Configurações ---
LiquidCrystal_I2C lcd(0x27, 16, 2);
Servo travaCofre;
const int pinoServo = 11;
const int pinoMoedeiro = 2;
const int tempoLimiteSemPulsos = 500;
volatile int contadorPulsos = 0;
long tempoUltimoPulso = 0;
const int buzzerPin = 12;
const int led1 = 32;
const int led2 = 34;
const int led3 = 36;
const int led4 = 38;

// --- Configuração do Teclado 4x3 ---
const byte ROWS = 4;
const byte COLS = 3;
char keys[ROWS][COLS] = {
  {'1','2','3'},
  {'4','5','6'},
  {'7','8','9'},
  {'*','0','#'}
};
byte rowPins[ROWS] = {9, 8, 7, 6};
byte colPins[COLS] = {5, 4, 3};
Keypad customKeypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

// --- Struct de Configuração ---
struct Configuracao {
  byte magic_number;
  byte modoCofre;
  long valorTotal;
  long valorMeta;
  bool metaAtingida;
};
Configuracao config;

// --- Variáveis para Comandos ---
String bufferComando = "";
String codigoReset = "000#";
String codigoReconfig = "*#*";
String codigoSaque = "1#";
bool travaPendenteDeFechamento = false; // A BANDEIRA DE ESTADO DA TRAVA

// --- Funções ---

void setup() {
  lcd.init();
  lcd.backlight();
  travaCofre.attach(pinoServo);
  travaCofre.write(90);
  pinMode(buzzerPin, OUTPUT);
  pinMode(led1, OUTPUT);
  pinMode(led2, OUTPUT);
  pinMode(led3, OUTPUT);
  pinMode(led4, OUTPUT);

  EEPROM.get(0, config);

  if (config.magic_number != 123) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Bem-vindo!");
    delay(2000);
    config.valorTotal = 0;
    config.metaAtingida = false;
    iniciarConfiguracao();
  }

  pinMode(pinoMoedeiro, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(pinoMoedeiro), contaPulso, FALLING);
  
  atualizarDisplay();
}

void loop() {
  if (contadorPulsos > 0) {
    if (millis() - tempoUltimoPulso > tempoLimiteSemPulsos) {
      identificaMoeda();
      contadorPulsos = 0;
    }
  } 
  else {
    char key = customKeypad.getKey();
    if (key) {
      bufferComando += key;

      if (config.metaAtingida && bufferComando.endsWith(codigoSaque)) {
        bufferComando = "";
        realizarSaque();
      }
      else if (bufferComando.endsWith(codigoReconfig)) {
        bufferComando = "";
        iniciarConfiguracao();
      }
      else if (bufferComando.endsWith(codigoReset)) {
        resetTotalDoCofre();
      }

      if (bufferComando.length() > 10) {
        bufferComando = "";
      }
    }
  }
}

// --- Funções de Configuração ---

void iniciarConfiguracao() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Escolha o Modo:");
  delay(1500);

  config.modoCofre = 0;
  while (config.modoCofre != 1 && config.modoCofre != 2) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("1-Com Meta");
    lcd.setCursor(0, 1);
    lcd.print("2-Sem Meta");
    
    char key = customKeypad.waitForKey();
    if (key == '1') {
      config.modoCofre = 1;
    } else if (key == '2') {
      config.modoCofre = 2;
    }
  }

  if (config.modoCofre == 1) {
    telaDeDefinirMeta();
  } else {
    config.valorMeta = 0;
  }

  config.magic_number = 123;
  config.metaAtingida = false;
  EEPROM.put(0, config);
  
  // Condicional para travar o cofre
  if (travaPendenteDeFechamento) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Travando cofre...");
    travarCofre();
    travaPendenteDeFechamento = false; 
    delay(1000);
  }
  
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Configuracao");
  lcd.setCursor(0, 1);
  lcd.print("Concluida!");
  delay(2000);
  atualizarDisplay();
}

void telaDeDefinirMeta() {
  String reaisDigitados = "";
  String centavosDigitados = "";
  long metaReais = 0;
  long metaCentavos = 0;
  char key;
  bool inserindoReais = true;

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Meta (Reais):");
  lcd.setCursor(0, 1);
  lcd.print("R$ ");

  while (inserindoReais) {
    key = customKeypad.waitForKey();
    if (isdigit(key)) {
      reaisDigitados += key;
      lcd.print(key);
    } else if (key == '*' && reaisDigitados.length() > 0) {
      reaisDigitados.remove(reaisDigitados.length() - 1);
      lcd.setCursor(3, 1);
      lcd.print("             ");
      lcd.setCursor(3, 1);
      lcd.print(reaisDigitados);
    } else if (key == '#' && reaisDigitados.length() > 0) {
      metaReais = atol(reaisDigitados.c_str());
      inserindoReais = false;
    }
  }

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Meta (Centavos):");
  lcd.setCursor(0, 1);
  lcd.print("R$ " + reaisDigitados + ".");

  while (true) {
    key = customKeypad.waitForKey();
    if (isdigit(key) && centavosDigitados.length() < 2) {
      centavosDigitados += key;
      lcd.print(key);
    } else if (key == '*' && centavosDigitados.length() > 0) {
      centavosDigitados.remove(centavosDigitados.length() - 1);
      lcd.setCursor(3 + reaisDigitados.length() + 1, 1);
      lcd.print("  ");
      lcd.setCursor(3 + reaisDigitados.length() + 1, 1);
      lcd.print(centavosDigitados);
    } else if (key == '#') {
      metaCentavos = (centavosDigitados.length() > 0) ? atol(centavosDigitados.c_str()) : 0;
      break;
    }
  }
  
  config.valorMeta = (metaReais * 100) + metaCentavos;
}


// --- Funções Principais de Operação ---

void identificaMoeda() {
  int valorMoeda = 0;
  switch (contadorPulsos) {
    case 1: valorMoeda = 100; break;
    case 2: valorMoeda = 50; break;
    case 3: valorMoeda = 25; break;
    case 4: valorMoeda = 10; break;
  }

  if (valorMoeda > 0) {
    config.valorTotal += valorMoeda;

    if (config.modoCofre == 1 && config.valorTotal >= config.valorMeta && !config.metaAtingida) {
      config.metaAtingida = true;
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("META ATINGIDA!");
      lcd.setCursor(0, 1);
      lcd.print("Parabens!");
      delay(3000);
    }
    
    EEPROM.put(0, config);
    ledMeta();
    atualizarDisplay();
  }
}

void atualizarDisplay() {
  lcd.clear();
  if (config.modoCofre == 1 && !config.metaAtingida) {
    lcd.setCursor(0, 0);
    lcd.print("R$");
    lcd.print(config.valorTotal / 100.0, 2);
    lcd.setCursor(0, 1);
    lcd.print("Meta: R$");
    lcd.print(config.valorMeta / 100.0, 2);
  } else {
    lcd.setCursor(0, 0);
    lcd.print("Total no Cofre:");
    lcd.setCursor(0, 1);
    lcd.print("R$ ");
    lcd.print(config.valorTotal / 100.0, 2);
  }
}

void destravarCofre() {
  travaCofre.write(0);
  delay(500);
  travaCofre.write(90);
}

void travarCofre() {
  travaCofre.write(180);
  delay(500);
  travaCofre.write(90);
}

void realizarSaque() {
  destravarCofre();
  travaPendenteDeFechamento = true; 
  
  tone(buzzerPin, 1000);
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("Retire o valor.");
  delay(4000);
  noTone(buzzerPin);

  config.valorTotal = 0;
  
  iniciarConfiguracao();
}

void resetTotalDoCofre() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Resetando...");
  
  config.magic_number = 0; 
  EEPROM.put(0, config);
  
  delay(2000);

  void(* reinicio) (void) = 0;
  reinicio();
}

void contaPulso() {
  contadorPulsos++;
  tempoUltimoPulso = millis();
}

void ledMeta(){
  float porcentoled = 0.25;
  if (config.modoCofre == 1){
    digitalWrite(led1, LOW);
    digitalWrite(led2, LOW);
    digitalWrite(led3, LOW);
    digitalWrite(led4, LOW);

    if (config.valorTotal >= config.valorMeta * porcentoled)      digitalWrite(led1, HIGH);
    if (config.valorTotal >= config.valorMeta * (porcentoled * 2)) digitalWrite(led2, HIGH);
    if (config.valorTotal >= config.valorMeta * (porcentoled * 3)) digitalWrite(led3, HIGH);
    if (config.valorTotal >= config.valorMeta)                    digitalWrite(led4, HIGH);
  } else {
    digitalWrite(led1, LOW);
    digitalWrite(led2, LOW);
    digitalWrite(led3, LOW);
    digitalWrite(led4, LOW);
  }
}
