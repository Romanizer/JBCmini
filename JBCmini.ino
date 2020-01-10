#include <LiquidCrystal_I2C.h>

#define HEAT_PIN 5
#define TC_PIN A0
#define THERMISTOR_PIN A2   //old A1
#define ROT_A_PIN 3 
#define ROT_B_PIN A1        //old 7 btn_down
#define ZEROCROSS_PIN 2
#define HIBERNATION_PIN A7  //new //reconnect to A7 BODGE <- deprecated info
#define R8_RESISTANCE 97750.0
#define R9_RESISTANCE 255.07
#define VCC 5.0             //old 5.1 V

int tipTempIs = 0;
int tipTempIsDisplay = 0;   // Separate Display variables for asynchronous updates
int tipTempSet = 300;       
int tipTempSetSlave = 300;  // adjustable with encoder
int tipTempSetDisplay = 0;  // Separate Display variables for asynchronous updates
int tipTempHibernation = 150;
bool heat = false;
bool hibernation = false;
int mainsCycles = 0;
unsigned long buttonMillis = 0; // When last button press was registered

float getAmbientTemperature() {
  // Calculates °C from RTD voltage divider
  double Temp = log(10000.0 * ((1024.0 / analogRead(THERMISTOR_PIN) - 1)));
  Temp = 1 / (0.001129148 + (0.000234125 + (0.0000000876741 * Temp * Temp )) * Temp );
  return Temp - 273.15;
}

float runningAverage(float M) {
  #define LENGTH 20
  static int values[LENGTH];
  static byte index = 0;
  static float sum = 0;
  static byte count = 0;
  sum -= values[index];
  values[index] = M;
  sum += values[index];
  index++;
  index = index % LENGTH;
  if (count < LENGTH) count++;
  return sum / count;
}

void zeroCrossingInterrupt() {
  mainsCycles++;
  //TODO: Move heatpin on off here
  if (heat){
    digitalWrite(HEAT_PIN, HIGH);
  }else{
    digitalWrite(HEAT_PIN, LOW);
  }
}

//old serial LCD Init code, deprecated.
//LiquidCrystal lcd(A2, A3, A4, A5, 0, 1 );
LiquidCrystal_I2C lcd (0x3F, 2, 1, 0, 4, 5, 6, 7, 3, POSITIVE);

void setup()
{
  lcd.begin(16,2);
  lcd.setCursor(0,0);
  lcd.print("Set: ---");
  lcd.print(char(223));
  lcd.print('C');
  lcd.setCursor(0,1);
  lcd.print("Is:  ---");
  lcd.print(char(223));
  lcd.print('C');
  pinMode(HEAT_PIN, OUTPUT);
  pinMode(ROT_A_PIN, INPUT); //INPUT_PULLUP: no resistor R15
  pinMode(ROT_B_PIN, INPUT); //INPUT_PULLUP: no resistor R16
  pinMode(HIBERNATION_PIN, INPUT); //ROTARY_C //still need R17
  attachInterrupt(digitalPinToInterrupt(ZEROCROSS_PIN), zeroCrossingInterrupt, CHANGE);
}

void loop()
{
  
  if (mainsCycles >= 0){ // At 0 turn off heater
    heat = false;
    //digitalWrite(HEAT_PIN, LOW);
    lcd.setCursor(15,0);
    lcd.print("L");
  }

  if (mainsCycles > 6){ // Wait for 6 more mains cycles to get an undisturbed reading
    noInterrupts();
    tipTempIs = round(runningAverage(((analogRead(TC_PIN)*(VCC/1023.0))/(1+R8_RESISTANCE/R9_RESISTANCE))*43500+getAmbientTemperature()));
    
    if (tipTempIs < tipTempSet){ // If heat is missing
      heat = true;
      //digitalWrite(HEAT_PIN, HIGH); //replace with heat = true;
      mainsCycles = sqrt(tipTempSet - tipTempIs)*-1; // Schedule next measurement
      interrupts();
      lcd.setCursor(15,0);
      lcd.print("H");
      //noInterrupts();
    }
    else{ // If no heat is missing
      mainsCycles = -4;
    }
    interrupts();
  }
  
  if (abs(tipTempIs-tipTempIsDisplay) >= 1) // Is it time to update the display?
  {
    noInterrupts();
    tipTempIsDisplay = tipTempIs;
    interrupts();
    lcd.setCursor(5,1);
    lcd.print("   ");
    lcd.setCursor(5,1);
    lcd.print(tipTempIsDisplay);
    //interrupts();
  }

  if (abs(tipTempSet-tipTempSetDisplay) >= 1) // Is it time to update the display?
  {
    noInterrupts();
    tipTempSetDisplay = tipTempSet;
    interrupts();
    lcd.setCursor(5,0);
    lcd.print("   ");
    lcd.setCursor(5,0);
    lcd.print(tipTempSetDisplay);
    //interrupts();
  }
  
  //Rotary Encoder CODE      runs well, might tweak later
  if (!digitalRead(ROT_A_PIN) && tipTempSetSlave <= 400 && millis() > buttonMillis+75){

    if(!digitalRead(ROT_B_PIN) && tipTempSetSlave > 0){
      tipTempSetSlave = tipTempSetSlave - 10;
    }else if(digitalRead(ROT_B_PIN) && tipTempSetSlave < 400){
      tipTempSetSlave = tipTempSetSlave + 10;
    }
    buttonMillis = millis();
  }

  //HIBERNATION CODE
  if ((analogRead(HIBERNATION_PIN) < 512) && !hibernation){
    hibernation = true;
    lcd.setCursor(11,1);
    lcd.print("Sleep");
  }else if ((analogRead(HIBERNATION_PIN) > 512) && hibernation){
    hibernation = false;
    lcd.setCursor(11,1);
    lcd.print("     ");
  }

  //TODO: Optimize!
  if (!hibernation){
    tipTempSet = tipTempSetSlave;
  }else{
    tipTempSet = tipTempHibernation;
  }
}
