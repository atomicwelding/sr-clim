#include <Adafruit_MAX31865.h>
#include <math.h>

#define TIME_LOOP 2000

#define RTDPINSSOCK 30,31,32,33
#define RTDPINSSCIENCE 50,51,52,53 

#define RREF 430.0
#define RNOMINAL 100.0

/*
 * Prise en charge des deux capteurs PT100 
 * On appelle la méthode temperature(RNOMINAL, RREF) pour en lire les valeurs 
 */
 
Adafruit_MAX31865 rtdsock = Adafruit_MAX31865(RTDPINSSOCK);
Adafruit_MAX31865 rtdscience = Adafruit_MAX31865(RTDPINSSCIENCE);

void dac(float voltage) {
  if(voltage > 6)
    voltage = 6.0;
  else if(voltage < 0)
    voltage = 0.0;

  
  analogWrite(DAC0, to_int(voltage));
}

int to_int(float voltage) {
  return (int) (voltage/7.01 * 4095);
}

/*
 * Le 1er étage désigne la boucle de régulation "interne" 
 * Régule la température en sortie de chaussette
 * Toutes les variables et fonctions sont précédées de pr.
 * 
 * Le 2eme étage désigne la boucle de régulation "externe"
 * Régule la température sur la table de sciences
 * Toutes les variables et fonctions sont précédées de de.
 */

float valve_default = 2.5;
float time_since_start = 0.0;

struct PremierEtage {
  float consigne;
  float consigne_base;
  float mesure;
  float prev_mesure;
  float cmd;

  float Kp;
  float Ti;
  float Td;

  float erreur;
  float erreur_integree;
  float erreur_derivee;
};

PremierEtage pr = {0.0, 19.5, 0.0, 0.0, 0.0,
                   0.2, 800.0, -120.0,
                   0.0, 0.0, 0.0};

float cmd_to_valve(float x) {
  float a = 0.5;
  float expon = 2.0;
  return a * powf(max( 0.f, powf(valve_default/a, 1/expon) + x ), expon);
}

struct DeuxiemeEtage {
  float consigne;
  float mesure;
  float cmd;

  float Kp;
  float Ti;

  float erreur;
  float erreur_integree;
  float erreur_derivee; 
};

DeuxiemeEtage de = {23.5, 0.0, 0.0,
                    0.1, 600, 
                    0.0, 0.0, 0.0};


float temp_diff_array[10] = {0.0, 0.0, 0.0, 0.0, 0.0,
                        0.0, 0.0, 0.0, 0.0, 0.0};

int count_forarray = 0;


float mean_array() {
  float res = 0.0; 
  for(int i = 0; i < 10; i++) {
     res += temp_diff_array[i];
  }

  return res/10;
}

void setup() {
  // initialisation des deux capteurs, en mode 4 fils 
  rtdsock.begin(MAX31865_4WIRE);
  rtdscience.begin(MAX31865_4WIRE);

  // résolution utilisée pour le DAC
  analogWriteResolution(12);

  Serial.begin(115200);

  pr.consigne = pr.consigne_base;
}

void loop() {
  //// 1ER ETAGE ////
  pr.prev_mesure = pr.mesure;
  pr.mesure = rtdsock.temperature(RNOMINAL, RREF);
  
  pr.erreur = pr.consigne - pr.mesure;
  float last_I = pr.erreur_integree;
  pr.erreur_integree += TIME_LOOP/1000 * pr.erreur;
  
  //////////////
  temp_diff_array[count_forarray] = pr.mesure - pr.prev_mesure;
  count_forarray++;
  
  if(count_forarray >= 10)
    count_forarray = 0;
    
  pr.erreur_derivee = mean_array() / TIME_LOOP;

  ////////////////
  
  pr.cmd = -( pr.Kp * pr.erreur + 1/pr.Ti * pr.erreur_integree + pr.Td * pr.erreur_derivee );
  pr.cmd = cmd_to_valve(pr.cmd);

  // permet d'éviter que l'asservissement fasse brutalement varier la
  // température lorsqu'il vient d'être démarré
  float r = time_since_start / (pr.Ti / 2.);
  if(r < 1)
    pr.cmd = r * pr.cmd + (1-r) * valve_default;
  time_since_start += TIME_LOOP/1000;

  // on surveille la saturation de l'intégrateur et on borne la commande de sortie
  // la plage utile ne dépassant pas 6 volts
  if(pr.cmd > 6) {
    pr.cmd = 6.0;
    pr.erreur_integree = last_I; 
  } else if(pr.cmd < 0) {
    pr.cmd = 0.0;
    pr.erreur_integree = last_I;
  }

  //// SECOND ETAGE ////

  de.mesure = rtdscience.temperature(RNOMINAL, RREF);

  de.erreur = de.consigne - de.mesure;
  last_I = de.erreur_integree;
  de.erreur_integree += TIME_LOOP/1000 * de.erreur;

  de.cmd = de.Kp * de.erreur + 1/de.Ti * de.erreur_integree;
  pr.consigne = pr.consigne_base + de.cmd;

  // on surveille la saturation de l'intégrateur et on fait en sorte 
  // que la consigne de la chaussette ne diverge pas de la consigne
  // sur la table de science
  if(pr.consigne > de.consigne) {
    pr.consigne = de.consigne;
    de.erreur_integree = last_I;
  } else if (pr.consigne < 17) {
    pr.consigne = 17.0;
    de.erreur_integree = last_I;
  }

  dac(pr.cmd);

  String pt_tsock = "TSOCK:" + String(rtdsock.temperature(RNOMINAL, RREF), 3);
  String pt_ttable = "TTABLE:" + String(rtdscience.temperature(RNOMINAL, RREF), 3);
  
  String pt_cmd = "CMD:" + String(pr.cmd, 3);
  String pt_consigne = "CONS:" + String(pr.consigne, 3);

  Serial.println(pt_tsock + "-" + pt_ttable + "-" + pt_cmd + "-" + pt_consigne);  
  delay(TIME_LOOP);
}
