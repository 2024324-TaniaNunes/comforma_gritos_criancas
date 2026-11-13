#include <Arduino.h>
#include <Adafruit_NeoPixel.h>

const int PINO_SENSOR = A0;   // sensor de som (Loudness)
const int PINO_LEDS = 6;      // tira de LEDs (porta D6)
const int NUM_LEDS = 30;      // número de LEDs da tira

const int MARGEM = 60;                    // quanto acima do pico do silêncio conta como barulho
const unsigned long TEMPO_ACESO = 300;    // fica aceso 300 ms depois do barulho
const unsigned long TEMPO_IGNORAR = 150;  // ignora o sensor 150 ms depois de mudar as luzes

Adafruit_NeoPixel tira(NUM_LEDS, PINO_LEDS, NEO_GRB + NEO_KHZ800);

int limite = 0;
unsigned long ultimoBarulho = 0;
unsigned long ultimaMudanca = 0;  // quando as luzes mudaram pela última vez
bool ligado = false;

// média de várias leituras (reduz o ruído)
int lerSensor()
{
    long soma = 0;
    for (int i = 0; i < 8; i++)
    {
        soma += analogRead(PINO_SENSOR);
    }
    return soma / 8;
}

void ligarLuzes()
{
    for (int i = 0; i < NUM_LEDS; i++)
    {
        tira.setPixelColor(i, tira.Color(0, 0, 255));  // azul
    }
    tira.show();
    ultimaMudanca = millis();
}

void desligarLuzes()
{
    tira.clear();
    tira.show();
    ultimaMudanca = millis();
}

void setup()
{
    Serial.begin(9600);
    tira.begin();
    tira.setBrightness(20);  // brilho baixo para gastar pouca corrente
    tira.show();             // começa tudo apagado

    delay(500);  // deixa a alimentação estabilizar

    // CALIBRAÇÃO: pico do silêncio durante 2 segundos (luzes apagadas)
    int maximo = 0;
    unsigned long inicio = millis();
    while (millis() - inicio < 2000)
    {
        int leitura = lerSensor();
        if (leitura > maximo)
        {
            maximo = leitura;
        }
        delay(10);
    }
    limite = maximo + MARGEM;

    Serial.print("Pico do silencio: ");
    Serial.print(maximo);
    Serial.print("  Limite: ");
    Serial.println(limite);
}

void loop()
{
    // logo depois de mudar as luzes, não lê o sensor
    if (millis() - ultimaMudanca < TEMPO_IGNORAR)
    {
        return;
    }

    int loudness = lerSensor();
    Serial.println(loudness);

    if (loudness > limite)
    {
        ultimoBarulho = millis();
        if (!ligado)
        {
            ligarLuzes();
            ligado = true;
        }
    }

    if (ligado && (millis() - ultimoBarulho > TEMPO_ACESO))
    {
        desligarLuzes();
        ligado = false;
    }

    delay(10);
}