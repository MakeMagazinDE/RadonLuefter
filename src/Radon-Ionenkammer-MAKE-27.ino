 //////////////////////////////////////////////////////////////////////////////
// Ionenkammer
// mit dem Arduino Nano
// 
// veröffentlicht in der MAKE 2/2026
//
// Ulrich Schmerold
// 03/2026
//////////////////////////////////////////////////////////////////////////////
 #define Programm_name "IonenKammer"

#include <Wire.h>               // Bibliothek u.a. für das I2C Protokoll
#include <LiquidCrystal_I2C.h>  //Bibliothek für das I2C KCD-Display
#include <GyverBME280.h>        //Bibliothek für den Temperatursensor bmp280
#include <SD.h>
 
GyverBME280 bme;
//LiquidCrystal_I2C lcd(0x27,16,2);  // LCD-Display mit 16 Zeichen und 2 Zeilen, I2C-Addresse: 0x27  
LiquidCrystal_I2C lcd(0x27,20,4);    // LCD-Display mit 20 Zeichen und 4 Zeilen, I2C-Addresse: 0x27 
//LiquidCrystal_I2C lcd(0x3F,16,2);  // LCD-Display mit 16 Zeichen und 2 Zeilen, I2C-Addresse: 0x3F  

#define file_name "/Radon.txt"
#define file_name2 "/Cycles.txt"

#define chipSelect 10   // Pin für die SD-Karte
#define Kammer_pin A0   // analoger Pin für die Spannung am Kammerdraht
#define Jumper_pin 4    // Pin, an dem der Jumper angeschlossen ist
#define Schalt_pin 3    // Pin, der den Lüfter schaltet
#define Heizung_pin 6   // Optional: Pin, an dem die Testkammer-Heizung angeschlossen ist

#define Korrekturwert_Temperatur 0  // Wert ändern, wenn die ausgelesene Temperatur nicht korrekt ist und angepasst werden soll
#define Korrekturwert_Luftfeuchte 0 // Wert ändern, wenn die ausgelesene Luftfeuchtigkeit nicht korrekt ist und angepasst werden soll
#define Korrekturwert_Druck 64      // Wert ändern, wenn der ausgelesene Druck nicht korrekt ist und angepasst werden soll

#define Anzahl_MW 10           // Aus wie vielen Werten wird der Mittelwert gebildet

// Arduino Nano 5V, ESP32 3.3V
#define LogicLevel      5.0   
 // Arduino Nano: 1023, ESP32 4095
#define adcRange        1023   
// Arduino Nano 3.0  / ESP32 2.3
#define Schwelle_JFET   3.0    

#define Aktivitaet_ein 300  // Bei welchem Aktivitätswert wird der Lüfter eingeschaltet;
#define Aktivitaet_aus 200  // Bei welchem Aktivitätswert wird der Lüfter ausgeschaltet;

//TTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTT   Temperatur Kompensation   TTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTT
  float Temperatur_Korrektur[21] ={254.41, 245.18, 217.67, 187.42, 182.7, 169.10, 153.96, 138.49, 124.59, 115.95, 111.05, 103.29, 94.88, 87.59, 79.62, 71.34, 65.21, 60.5, 56.09, 51.34, 47.6}; // Temperatur Array( 10°C - 30°C)
//TTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTT
  #define A_ref    3700               // Referenz-Aktivität mit RadonEye ermittelt
  #define Zz_Test  55.46              // Zykusdauer bei Sättigung der Testkammer / unbedingt ein .0 anhängen, da sonst der Wert nicht als float erkannt wird
  #define Offset    0                 // Verschiebung der Kurve - wird aus   Zz_Test errechnet
//===========================================================================================================
  #define Temperaturkompensation true    // Soll eine Kompensation der Temperatur erfolgen?
  float Zz0 = Temperatur_Korrektur[10];  // Zyklusdauer bei 20°C
  float Bqs = 229.4 ;                    // Aktivität pro Sekunde Zyklusdauer / Wert wird bei Setup() ermittelt
//===========================================================================================================
  byte Serial_Output_Art = 5;   //  0 = nichts ausgeben, 
                                //  1 = Spannung_Kammerdraht, 
                                //  2 = Zyklendauer, 
                                //  3 = Alle Werte ausgeben, 
                                //  4 = Alle Werte mit Einheiten,
                                //  5 = Zyklendauer und Aktivitätswert (Seriell und auf SD) ==> gut für Serial Plotter ==> Sättigungskurve
                                //  6 = Temperatur x 10 und Zyklenzeit (für Temperatur Kalibrierung)

  byte Grad[8] = {B00111,B00101,B00111,B0000,B00000,B00000,B00000,B00000};      // Sonderzeichen Grad(°) definieren für LCD-Display
  byte Durchschnitt[8] = { B00001, B00101, B01010, B10011, B10101, B01010, B10100, B10000 }; // Sonderzeichen Durchschnitt

  unsigned int   cycles = 0;  // Wie viele Werte wurden bisher ermittelt
  float Mittelwert = 0.0;     // Variable um den Mittelwert zu speichern
  float A = 0.0;              // Aktivität in Becqarels
  
  float Mittelwert_array[10]; // Variable, um Anzahl_MW Zyklen zu speichern und daraus den Mittelwert berechnen
  bool  Jumper = false;       // Hardware-Jumper für Serielle Ausgabe aller Spannungswerte oder nur der Zyklen-Zeit
  int   adcValue = 0;         // Variable für den analogen Wert vom Kammerdraht
  unsigned long lastHeizung;  // Damit die Heizung (falls vorhanden) nur 1 mal pro Sekunde abgefragt wird
  unsigned long timeStart;    // Variablen für die Zeitmessung
  unsigned long timeStop;     //        "
  float Zz = 0.0 ;            //        "
  float volt = 0.0;           // Diese Variable erhält den Spannungs-Wert vom Kammerdraht
 //---------------------------------------------------------------------------------------------------
  float t1 ;                  // Variable für die Temperatur
  unsigned int druck;         // Variable für den Luftdruck

void setup() //------------------------------------------ Startroutinen ---------------------------------------------
{
 Serial.begin(9600);
//───────────────────────────────────────────────────────────────────
  Bqs  =  A_ref / (Zz0 - Zz_Test);  // Formel zur Ermittlung der Aktivität pro Sekunde Zyklusdauer  
//───────────────────────────────────────────────────────────────────
  for (byte i = 0 ; i < Anzahl_MW ; i++) Mittelwert_array[i] = 0.0;
  pinMode(Jumper_pin, INPUT_PULLUP);  // pin, an den der Jumper angeschlossen ist als Input
  pinMode(Schalt_pin, OUTPUT);        // pin, an den das Lüfterrelais angeschlossen wird als Output
  digitalWrite(Schalt_pin, LOW);      // Lüfter ausschalten
  pinMode(Heizung_pin, OUTPUT);       // pin, an dem die Heizung angeschlossen ist als Output
  digitalWrite(Heizung_pin, HIGH);    // Heizung ausschalten ==> HIGH = Aus, LOW = An
  bme.begin();                        // Sensor (BME280) für Temperatur, Luftfeuchte und Luftdruck 
  LCD_Start();                        // Das LCD-Display initialisieren
  SD_init();                          // SD-Karte initialisieren
 // Test_pin(Schalt_pin);             // Lüfter-pin testen
 // Test_pin(Heizung_pin);            // Heizungs-pin testen
}

void loop() //-------------------------------------------- Loop -----------------------------------------------------
{
  LCD_loesche_Zeile(0 , 2);       // Am LCD-Display die Zeilen 0, 1 und 2 löschen
  show_climate();                 // Temperatur, Luftfeuchtigkeit und Luftdruck anzeigen
  show_Werte();                   // den Mittelwert berechnen und auf dem LCD ausgeben

 // --------------- die Kammer initialisieren --------------------------------------------------------------------------
  volt = 0;                       // den Spannungswert auf 0V zurücksetzen (sonst wird die while-Schleife nach einem Durchlauf nie wieder aufgerufen
  pinMode(Kammer_pin, OUTPUT);    // der Kammer-pin wird als Ausgang deklariert
  digitalWrite(Kammer_pin, LOW);  // der Kammer-pin wird auf GND (Masse) geschaltet. Damit wird der Kammerdraht entladen
  delay(1000);                    // eine Sekunde warten, damit die Kammer auch sicher entladen ist
  digitalWrite(Kammer_pin, HIGH); // Kammer-pin auf VCC setzen
  pinMode(Kammer_pin, INPUT);     // Den Kammer-pin jetzt umstellen auf Eingang
  timeStart = millis();           // Die Zeitmessung starten
//------------------------------------------------------------------------------------------------------------------------------------------- //
//                                         Ionenkammer auslesen                                                                               //
//--------------------------------------------------------------------------------------------------------------------------------------------//
 cycles++;                                                  // Wie viele Werte (komplette Durchläufe) wurden bisher schon ermittelt           //
 while(volt < Schwelle_JFET)                                // diese Schleife wird wiederholt, bis 3V erreicht werden -------------           //
 {                                                                                                                                            //
  adcValue = analogRead(Kammer_pin);                        // den Kammerwert auslesen                                                        //
  volt = float(adcValue * LogicLevel / adcRange);           // Arduino Nano: Den analogen Wert (0-1023) in eine Spannung (0-5V) umrechnen     //
                                                            // ESP32: Den analogen Wert (0-4095) in eine Spannung (0-2.3) umrechnen           //
  timeStop = millis();                                      // aktuelle Zeit auslesen                                                         //
  Zz = (timeStop - timeStart) / 1000.0;                     // wie lange hat es bisher schon gedauert (in Sekunden)                           //
  show_Volt();                                              // die aktuelle Zeitdauer und die momentane Spannung am Display anzeigen          //
  if (Serial_Output_Art == 1) Serial.println(String(volt)); //aktuelle Spannung am Kammerdraht seriell ausgeben                               //
  Heizung();                                                // Falls vorhanden Heizung ansteuern um eine Testkammer auf 20°C zu halten        //
  delay(100);                                               // 0.1 Sekunde warten, somit 10 Abfragen des Kammerdrahts pro Sekunde (sehr genau)//
  show_climate();                                           // Temperatur, Luftfeuchtigkeit und Luftdruck auslesen und anzeigen               //
 }                                                                                                                                            //
 // --------------- kommt das Programm an diese Stelle, so wurde die Schwelle_JFET erreicht, anderenfalls zurück zu while()-------------------//
   
   Mittelwert = berechne_mittelWert(Zz);
   A = Zeit_zu_Bq(Zz, t1, druck);
   Werte_ausgeben_und_speichern();
   doLuefter (A);                  // Radon-Lüfter schalten -
    
}// -------------- ein Durchlauf wurde komplett beendet. Der Arduino startet den nächsten Durchlauf (zurück zu loop) -------------------------------


void show_Volt()//-------------------- Zyklenzeit und Spannung am Kammerdraht auf dem LCD anzeigen
{
 lcd.setCursor(0,0);  lcd.print("Zz=" + String(Zz) + "s");
 lcd.setCursor(11,0); lcd.print("U=" + String(volt) + "V");
}

void show_climate()  // ---------------------------- Ausgabe der Werte vom BME280
{
  t1 = bme.readTemperature() + Korrekturwert_Temperatur;      // Temperatur auslesen und unter „t1“ speichern
  druck = int(bme.readPressure()/100) + Korrekturwert_Druck;  // Luftrdruck auslesen und unter „druck“ speichern (in Hektopascal)
 if (Serial_Output_Art != 5)
 {
  lcd.setCursor(0,3);
  lcd.print (t1);
  lcd.write((uint8_t)0); // Sonderzeichen °  
  lcd.print("C"); 
   
  lcd.setCursor(11,3);
  lcd.print(druck);
  lcd.print("hPa"); 
 } else {
  lcd.setCursor(11,2);
  lcd.print("T=");
  lcd.print (t1);
  lcd.write((uint8_t)0); // Sonderzeichen °  
  lcd.print("C");  
 }
}


void show_Werte() //--- Den Mittelwert der Cyklenzeit, die bisherigen Durchläufe (Cycles) und die Aktivität(Bq) auf dem Display ausgeben
{
  lcd.setCursor(0,1); 
    lcd.print("MW=" + String(Mittelwert) + "s");   
    lcd.setCursor(11,1);
    lcd.print("z="  + String(cycles));            

    lcd.setCursor(0,2); 
    lcd.print ("Bq=");  
    lcd.print(String(int(A))); 
}

void LCD_Start()  //--------------- das LCD-Display initialisieren
{
  lcd.init();
  lcd.createChar(0, Grad);
  lcd.createChar(1, Durchschnitt);
  lcd.backlight();
  lcd.setCursor(4,0);
  lcd.print(Programm_name);
  delay(2000);
}

//----------------------------------------- Mittelwert aus Anzahl_MW Werten berechnen
float berechne_mittelWert(float neuerWert) 
{
  byte i;
  for(i = 0 ; i < Anzahl_MW ; i++) if (Mittelwert_array[i] <= 0) Mittelwert_array[i] = neuerWert; else Mittelwert_array[i] = Mittelwert_array[i+1];
  Mittelwert_array[Anzahl_MW-1] = neuerWert;
  float MW = 0;
  for(i = 0 ; i < Anzahl_MW ; i++) MW = MW + Mittelwert_array[i];
  MW = MW / Anzahl_MW;
  return MW;
}


//                                     ╔═════════════════════════════════════════════════════╗
//   ═════════════════════╣    Umwandlung der Zyklenzeit in Becquerel - mit Temperatur- und Druckkompensation    ╠══════════════════════════════════════
//                                     ╚═════════════════════════════════════════════════════╝
int Zeit_zu_Bq (float Zz, float T, int p)
{ 
 //Zz = (timeStop - timeStart) / 1000.0; 
//
//        ╔══════════════════════════════════╗                                                                              
               A = (TK(T) - Zz) * Bqs *  ( 1013.0 / p ) + Offset;                     
//        ╚══════════════════════════════════╝                                                                      
//
if (A < 0.0) A = 0.0;         //negative Strahlung gibt es bei mir nicht nicht ==> Messfehler
if (A > 9999.0) A = 9999.0;   // Da stimmt etwas nicht ?!?!?!
  return int(A);
} 
//══════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════


float TK(float Temperatur)//----------------------------------- Korrektur der Zyklenzeit, wenn die Temperatur mit Nachkommazahl verwendet wird. Z.B. wird aus (12.47°C / 209.55sec) ==> (12.0°C / 201.65sec)
{  
  float Abstand, Rest;
  int Temperatur_gerundet = round(Temperatur);
  byte index = round(Temperatur)-10;
  if (Temperaturkompensation == false) return Temperatur_Korrektur[10]; // Keine Temperaturkompensation anwenden ==> Return Zz0
 
  if ( Temperatur < 10 ) return Temperatur_Korrektur[0]  + ((Temperatur_Korrektur[0] - Temperatur_Korrektur[1])) * (10 - Temperatur) ;
  if ( Temperatur > 30 ) return Temperatur_Korrektur[20] - ((Temperatur_Korrektur[19]- Temperatur_Korrektur[20])) * (Temperatur - 30);

  if (Temperatur >= Temperatur_gerundet)  
  {
     Rest = Temperatur - Temperatur_gerundet;
     Abstand = Temperatur_Korrektur[index] - Temperatur_Korrektur[index+1];
    return Temperatur_Korrektur[index] - Abstand * Rest ;
  }
  if (Temperatur < Temperatur_gerundet)  {
      Rest = Temperatur_gerundet - Temperatur ;
      Abstand = Temperatur_Korrektur[index-1] - Temperatur_Korrektur[index];
    return Temperatur_Korrektur[index] - Abstand * Rest ;
  }
  Serial.println(F("Es ist ein unbekannter Fehler aufgetreten bei ==>float TK(float Temperatur)" ));
  return 300; 
}

void Werte_ausgeben_und_speichern()
{
   String t2 = ersetze_Punkt_gegen_Komma(t1); // Zeitwert aufbereiten für Verwendung in *.csv - Datei
   
// Hinweis: Serial_Output_Art == 1 gibt die aktuelle Spannung am Kammerdraht aus / wird bei der Kammerfunktion (loop) abgerufen
   if (Serial_Output_Art == 2)  Serial.println(F("Test/Platzhalter"));
   if (Serial_Output_Art == 3)  Serial.println("T=" + String(t1) + "  TK=" + String (TK(t1)) + "  Bqs=" +String (Bqs) + "  Zz=" + String(Zz) + "  Mitelwert Zz="  +  String(Mittelwert) +  "  A=" + String(A));
   if (Serial_Output_Art == 4)  SD_speichern( String (cycles)+ "; " + String(int(Zz))+"; " +String(int(Mittelwert))+"; " +String(int(A)) + "; " + t2 + "; " +  String(druck));
   if (Serial_Output_Art == 5)  SerialPrint_Cycle_Kompressed(10, Zz, A);      // Zur Kalibrierung,ZyclusZeit Ausgabe auf Serial Plotter und Speicherung auf SD
   if (Serial_Output_Art == 6)  Kalibrierung (t1, Zz);
}


void Kalibrierung (float T, float Zz) //----------------- Zur Temperaturkompensation
{
  static float Summe_Zz = 0.0;
  static float Summe_T = 0.0;
         float Ergebnis;
           int index = 0;
  static int   index_saved = 0; 
  static byte  n = 0;
  static bool  start = true;

  index = round(T)-10;
  if (start ==  true) {index_saved = index; start = false;Serial.println(F("Start Kalibrierung")); }
   
   if ((T > 9.0) and (T < 31.0) )
   {
    if ( (index > index_saved) and (n>0)) 
    {
      Summe_Zz = Summe_Zz / n;
      Summe_T = Summe_T / n;
      Ergebnis = Summe_Zz / Summe_T * (index_saved + 10);
      Serial.println(F("...........................................................................................")); 
        Serial.println("   ==> Index: " + String(index_saved) + " / " + String(index_saved + 10) +  "°C    ==> ZyklusZeit(Zz): " + String(Ergebnis) + " <==    (gemittelt aus " + String(n) + " Wertepaaren)");
      Serial.println (F("__________________________________________________________________________________________"));   
      n = 1;
      Summe_Zz =  Zz;
      Summe_T  =  T;    
      Serial.println(("Index = ") + String(index) + " n=" + String(n)+ ("  ZyklusZeit = ") + String(t1 * Zz / round(t1)) + "   (" + String(t1) + "°C / " + String(Zz) +"sec)"  );      
      index_saved = index;
    } else 
    {
     n++;
     Summe_Zz = Summe_Zz + Zz;
     Summe_T  = Summe_T + T;
     Serial.println(("Index = ") + String(index) + " n=" + String(n)+ ("  ZyklusZeit = ") + String(t1 * Zz / round(t1)) + "   (" + String(t1) + "°C / " + String(Zz) +"sec)"  );     
    }
   } 
}

void SerialPrint_Cycle_Kompressed(byte Kompressions_Rate, float wert, float A) // "Kompression" Werte werden zusammengefasst und daraus der Mittelwert gebildet.
                                                                               // Dadurch können Zyklenzeiten (und Aktivität) über einen längeren Zeitraum übersichtlich im Serial Plotter dargestellt werden
{  
  static float Cyl = 0.0;
  static unsigned int AA = 0;
  static unsigned int Temperatur_100 = 0;
  static byte n = 0 ;
  static float Cyl_min = 1000.0;
  static int AA_max = 0;

  if( n < Kompressions_Rate)
  { 
    Cyl = Cyl + wert; 
    AA = AA + A;
    Temperatur_100 = Temperatur_100 + (100 * t1);
    n++;
  }

  if( n >= Kompressions_Rate)
  {
    if ((Cyl / n) < Cyl_min) Cyl_min = (Cyl / n ); // Kürzeste Zyklendauer erfassen
    if ((AA  / n) > AA_max ) AA_max  = (AA  / n ); // Größte Aktivität erfassen
   
     Serial.print  (String(Cyl / n * 10));
     Serial.print  ("," + String(AA / n));
     Serial.println("," + String(Temperatur_100 / n));

     //------------------ Auf die SD-Karte speichern
     SD_speichern_CycleTime(String(ersetze_Punkt_gegen_Komma(Cyl / n))  +  "; " + String(ersetze_Punkt_gegen_Komma(AA / n)) + "; "  +  String(ersetze_Punkt_gegen_Komma(Temperatur_100 / n / 100)));  

    //---------LCD Ausgabe --------------------
   if(Serial_Output_Art == 5)
   {
    lcd.setCursor(0,3);
    lcd.write((uint8_t)1);
    lcd.print ("Zmin=" + String(Cyl_min) + "sec");
   }

    n = 0;
    Cyl = 0.0;
    AA = 0.0;
    Temperatur_100 = 0;
  }
}


void doLuefter (float A) //-------------------------------- Den Lüfter ansteuern 
{
  if( A > Aktivitaet_ein) digitalWrite(Schalt_pin, HIGH);    // Lüfter EINschalten  
  if( A < Aktivitaet_aus) digitalWrite(Schalt_pin, LOW);     // Lüfter AUSschalten
}

String ersetze_Punkt_gegen_Komma(float val) // -------- Punkt gegen Komma ersetzen. Nötig für csv-Dateien
{
  String tmp;
  tmp = String(val);
  tmp.replace('.', ','); 
  return tmp;
}

void Heizung()  //------------- Wenn Heizung vorhanden, dann die Testkammer auf 20C° halten
{
  if ((lastHeizung + 1000) < millis())
  {
   lastHeizung = millis();
   int t20 = int( (bme.readTemperature() + Korrekturwert_Temperatur) * 10);       // Temperatur auslesen 
    if (t20 < 200) digitalWrite(Heizung_pin, LOW);                                // Heizung einschalten
    if (t20 >= 200) digitalWrite(Heizung_pin, HIGH);                              // Heizung ausschalten
  }  
}

void LCD_loesche_Zeile(byte Start ,byte Ende  )
{
  for( byte i = Start; i <= Ende; i++)
  { 
    lcd.setCursor(0,i);
    for (byte ii = 0; ii < 20 ; ii++)lcd.write(32);  //Leerzeile
  }  
}


void Test_pin(byte pin) // --------------------  Testen ob der angegebene Pin funktioniert, 5 mal Blinken
{
  for (byte i = 0 ; i < 5 ; i++)
  {
    digitalWrite(pin, LOW);
    delay (500);
    digitalWrite(pin, HIGH);
    delay(500);
  }
}
//▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓ ENDE ▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓
