 //////////////////////////////////////////////////////////////////////////////
// Ionenkammer
// mit dem Arduino Nano
// 
// veröffentlicht in der MAKE 2/2026
//
// Ulrich Schmerold
// 03/2026
//////////////////////////////////////////////////////////////////////////////

void SD_init()  // -----------  SD-Karte initialisieren
{
  byte Versuch = 0;
  
  while( (!SD.begin(chipSelect)) and (Versuch < 5))
  {
    Versuch++;
   lcd.setCursor(0,2);
   lcd.print(F("Init SD - Versuch "));
   lcd.print(Versuch);
  }  
 if (Versuch < 5) 
 {
   lcd.setCursor(2,2);
   lcd.print(F("SD-Karte erkannt"));
   delay(2000);
  }else{
   lcd.setCursor(0,2);
   lcd.print(F("Keine SD-Karte !!! "));
   delay(2000);
  }
  lcd.clear();
 }


void SD_speichern(String f)
{
  File dataFile;
  if (not (SD.exists(file_name))) 
  {
    dataFile = SD.open(file_name, FILE_WRITE);
    dataFile.println( F("Nr; dT ; MW-dT;  Aktivität; t2; p;"));
  } else dataFile = SD.open(file_name, FILE_WRITE); 
  dataFile.println(f); 
  dataFile.close();
}


void SD_speichern_CycleTime(String f)
{
  File dataFile;
  if (not (SD.exists(file_name2))) 
  {
    dataFile = SD.open(file_name2, FILE_WRITE);
    dataFile.println( F("Periodendauer"));
  } else dataFile = SD.open(file_name2, FILE_WRITE); 
  dataFile.println(f); 
  dataFile.close();
}
