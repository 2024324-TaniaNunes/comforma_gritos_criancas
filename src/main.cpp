#include <Arduino.h>
#include <Adafruit_NeoPixel.h>

// ---------- LIGAÇÕES ----------
const int PINO_SENSOR = A0;   // sensor de som (Loudness)
const int PINO_LEDS = 6;      // tira de LEDs (porta D6)
const int NUM_LEDS = 30;      // número de LEDs da tira

// ---------- AJUSTES DO SOM ----------
const int MARGEM = 60;                    // quanto acima do silêncio conta como som
const int ALCANCE = 100;                  // quanto o som tem de subir acima do limite para acender os 30 LEDs
const float VELOCIDADE_DESCIDA = 0.4;     // quantos LEDs apagam por ciclo quando o som baixa

// ---------- AJUSTES DAS CORES ----------
const int BRILHO = 20;            // 0 a 255
const int G_AMARELO = 130;        // quanto verde tem o amarelo
const int LARGURA_DEGRADE = 2;    // LEDs de transição em cada fronteira

// ---------- AJUSTES DA RECOLHA DE DADOS ----------
const unsigned long INTERVALO_DADOS = 250;   // escreve uma linha de dados a cada 250 ms
const unsigned long INTERVALO_AVISOS = 1000; // mínimo entre mensagens de aviso (evita repetir)

Adafruit_NeoPixel tira(NUM_LEDS, PINO_LEDS, NEO_GRB + NEO_KHZ800);

int limite = 0;              // valor do sensor a partir do qual há som
float nivelSuave = 0;        // nível atual da barra (em LEDs, com decimais)
int ultimoNumero = -1;       // quantos LEDs estavam acesos da última vez

// dados recolhidos
int maximoVisto = 0;                 // maior leitura desde o arranque
unsigned long ultimoDado = 0;        // quando escrevemos a última linha de dados
unsigned long ultimoAviso = 0;       // quando escrevemos o último aviso
int zonaAtual = 0;                   // 0 silêncio, 1 verde, 2 amarelo, 3 vermelho
int vezesVermelho = 0;               // quantas vezes o som chegou ao vermelho

// ---------- 1. LER O SENSOR ----------
int lerSensor()
{
    long soma = 0;
    for (int i = 0; i < 8; i++)
    {
        soma += analogRead(PINO_SENSOR);
    }
    return soma / 8;
}

// ---------- 2. CALIBRAR O SILÊNCIO ----------
void calibrar()
{
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

// ---------- 3. TRANSFORMAR O SOM EM NÚMERO DE LEDS ----------
float calcularAlvo(int loudness)
{
    if (loudness <= limite)
    {
        return 0;
    }
    float alvo = (float)(loudness - limite) * NUM_LEDS / ALCANCE;
    if (alvo > NUM_LEDS)
    {
        alvo = NUM_LEDS;
    }
    return alvo;
}

// ---------- 4. SUBIR RÁPIDO, DESCER DEVAGAR ----------
void atualizarNivel(float alvo)
{
    if (alvo > nivelSuave)
    {
        nivelSuave += (alvo - nivelSuave) * 0.6;
    }
    else
    {
        nivelSuave -= VELOCIDADE_DESCIDA;
        if (nivelSuave < alvo)
        {
            nivelSuave = alvo;
        }
    }
}

// ---------- 5. COR DE CADA LED ----------
uint32_t misturar(int r1, int g1, int r2, int g2, float f)
{
    int r = r1 + (r2 - r1) * f;
    int g = g1 + (g2 - g1) * f;
    return tira.Color(r, g, 0);
}

uint32_t corDoLed(int i)
{
    int fronteira1 = NUM_LEDS / 3;
    int fronteira2 = NUM_LEDS * 2 / 3;
    int meio = LARGURA_DEGRADE / 2;
    int inicio1 = fronteira1 - meio;
    int inicio2 = fronteira2 - meio;

    if (i < inicio1)
    {
        return tira.Color(0, 255, 0);
    }
    else if (i < inicio1 + LARGURA_DEGRADE)
    {
        float f = (float)(i - inicio1 + 1) / (LARGURA_DEGRADE + 1);
        return misturar(0, 255, 255, G_AMARELO, f);
    }
    else if (i < inicio2)
    {
        return tira.Color(255, G_AMARELO, 0);
    }
    else if (i < inicio2 + LARGURA_DEGRADE)
    {
        float f = (float)(i - inicio2 + 1) / (LARGURA_DEGRADE + 1);
        return misturar(255, G_AMARELO, 255, 0, f);
    }
    return tira.Color(255, 0, 0);
}

// ---------- 6. DESENHAR A BARRA ----------
void desenharBarra(int nAcesos)
{
    for (int i = 0; i < NUM_LEDS; i++)
    {
        if (i < nAcesos)
        {
            tira.setPixelColor(i, corDoLed(i));
        }
        else
        {
            tira.setPixelColor(i, 0);
        }
    }
    tira.show();
}

// ---------- 7. ZONA DO SOM (a mesma divisão da tira) ----------
// 0 = silêncio, 1 = verde (1-10 LEDs), 2 = amarelo (11-20), 3 = vermelho (21-30)
int zonaDe(int nAcesos)
{
    if (nAcesos == 0) return 0;
    if (nAcesos <= NUM_LEDS / 3) return 1;
    if (nAcesos <= NUM_LEDS * 2 / 3) return 2;
    return 3;
}

const char* nomeZona(int zona)
{
    if (zona == 0) return "silencio";
    if (zona == 1) return "verde";
    if (zona == 2) return "amarelo";
    return "vermelho";
}

// ---------- 8. MENSAGENS DE AVISO (só quando a zona muda) ----------
void verificarAvisos(int nAcesos)
{
    int zona = zonaDe(nAcesos);
    if (zona == zonaAtual)
    {
        return;  // nada mudou
    }
    if (millis() - ultimoAviso < INTERVALO_AVISOS)
    {
        return;  // mudou há pouco tempo: espera para não repetir avisos
    }

    ultimoAviso = millis();
    Serial.println();
    if (zona == 3)
    {
        vezesVermelho++;
        Serial.print(">>> AVISO: barulho muito alto! (vez n. ");
        Serial.print(vezesVermelho);
        Serial.println(")");
    }
    else if (zona == 2)
    {
        Serial.println(">>> ATENCAO: o barulho esta a aumentar.");
    }
    else if (zona == 1)
    {
        Serial.println(">>> Nivel de ruido normal.");
    }
    else
    {
        Serial.println(">>> Silencio.");
    }
    Serial.println();
    zonaAtual = zona;
}

// ---------- 9. LINHA DE DADOS EM TEMPO REAL ----------
// exemplo:  som:  95  max: 140  LEDs: 12/30  zona: amarelo  |############
void escreverDados(int loudness, int nAcesos)
{
    if (loudness > maximoVisto)
    {
        maximoVisto = loudness;
    }
    if (millis() - ultimoDado < INTERVALO_DADOS)
    {
        return;
    }
    ultimoDado = millis();

    Serial.print("som: ");
    Serial.print(loudness);
    Serial.print("  max: ");
    Serial.print(maximoVisto);
    Serial.print("  LEDs: ");
    Serial.print(nAcesos);
    Serial.print("/");
    Serial.print(NUM_LEDS);
    Serial.print("  zona: ");
    Serial.print(nomeZona(zonaDe(nAcesos)));
    Serial.print("  |");
    for (int i = 0; i < nAcesos; i++)
    {
        Serial.print("#");  // barra de texto: um # por LED aceso
    }
    Serial.println();
}

// ---------- 10. ENVIAR DADOS PARA O DASHBOARD (p5.js) ----------
// formato: D,leitura,nivel(0-100),ledsAcesos   (nivel = o mesmo da barra de LEDs)
void enviarDados(int loudness, int nAcesos)
{
    if (millis() - ultimoDado < 100)
    {
        return;  // 10 linhas por segundo
    }
    ultimoDado = millis();

    int nivel = (int)(nivelSuave * 100.0 / NUM_LEDS + 0.5);
    Serial.print("D,");
    Serial.print(loudness);
    Serial.print(",");
    Serial.print(nivel);
    Serial.print(",");
    Serial.println(nAcesos);
}

// ---------- PROGRAMA ----------
void setup()
{
    Serial.begin(9600);
    tira.begin();
    tira.setBrightness(BRILHO);
    tira.show();

    delay(500);
    calibrar();    // 2 segundos em silêncio!
    Serial.println("A recolher dados...");
}

void loop()
{
    int loudness = lerSensor();
    float alvo = calcularAlvo(loudness);
    atualizarNivel(alvo);

    int nAcesos = (int)(nivelSuave + 0.5);

    if (nAcesos != ultimoNumero)
    {
        desenharBarra(nAcesos);
        ultimoNumero = nAcesos;
        delay(10);
    }

    verificarAvisos(nAcesos);          // mensagens de aviso
    enviarDados(loudness, nAcesos);    // dados para o dashboard

    delay(20);
}