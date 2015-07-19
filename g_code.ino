/*
The MIT License (MIT)

Copyright (c) 2014 Jellyfush

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#define BAUD                (57600) 

#define MIN_FEEDRATE        (20)
#define MAX_FEEDRATE        (800)

#define MIN_STEPS_PER_MIN   (4000)
#define MAX_STEPS_PER_MIN   (80000)

#define IO_LED              (11)
#define STEPPER_DISABLE     (4)
#define STEPPER_STEP        (1)
#define STEPPER_DIRECTION   (0)
#define STEPPER_RESET       (3)
#define STEPPER_SLEEP       (2)

String sBuffer;

int stepsPerUnit = 87;
int feedRate = 100;
int stepDelay = 0;

void setup(){
  pinMode(STEPPER_DISABLE, OUTPUT);
  digitalWrite(STEPPER_DISABLE, 1); // disable stepper
  pinMode(STEPPER_STEP, OUTPUT);
  digitalWrite(STEPPER_STEP, 0);
  pinMode(STEPPER_DIRECTION, OUTPUT);
  digitalWrite(STEPPER_DIRECTION, 0);
  
  pinMode(STEPPER_RESET, OUTPUT); // I dont get why these are attached
  digitalWrite(STEPPER_RESET, 1);  
  pinMode(STEPPER_SLEEP, OUTPUT);
  digitalWrite(STEPPER_SLEEP, 1); 

  
  setAxisSteps(80); // default steps per mm
  setFeedRate(100); // default feed rate
  
  Serial.begin(BAUD);  // open coms
  
  printCommands(); 
  printReady();
}

void loop(){
  while(Serial.available() > 0) {
    char c = Serial.read(); 

    sBuffer += c;
    
    if(c == '\n' || c == '\r'){
      pushCurrentCommand();
      resetBuffer();
      printReady();
      break; 
    }    
    
  }
}

void resetBuffer(){
  sBuffer = "";
}

void printCommands(){
  Serial.println(F("Commands:"));
  Serial.println(F("G01 Z(units) [F(feedrate)]; - relative move"));
  Serial.println(F("G04 P(seconds); - delay (not implemented)")); // TODO, not needed yet
  Serial.println(F("G91; - relative mode"));
  Serial.println(F("M17; - enable motors"));
  Serial.println(F("M18; - disable motors"));
  Serial.println(F("M91 Z(steps); - Set axis steps per unit"));
  Serial.println(F("M100; - display commands"));
  Serial.println(F("M114; - report position and feedrate"));
}
void printReady(){
  Serial.print(F("\n\rok\n\r>")); 
}

boolean pushCurrentCommand(){
  if(sBuffer.length() < 3)// no commands are less than 3 char long
    return false; 
  
  String results[5];
  String attr;
  int resultSize = getCommandArray(results);

  char commandType = sBuffer[0];
  int id = getCommandAttr(results, resultSize, commandType).toInt();
  
  switch(commandType){
    case 'G':
      Serial.print(F("G command")); 
      switch(id){
        case 01:
          Serial.print(F(" Feed")); 
          attr = getCommandAttr(results, resultSize, 'F');
          if(attr != "")
            setFeedRate(attr.toFloat());
          attr = getCommandAttr(results, resultSize, 'Z');
          if(attr != "")
            return startMovement(attr.toFloat());
          return true;
        case 04:
          Serial.print(F(" Delay")); 
          return true;
        case 91:
          Serial.print(F(" Relative mode")); 
          return true;
      }
      return false;
    
    case 'M':
      Serial.print(F("M command")); 
      switch(id){
        case 17:
          Serial.print(F(" Enable"));
          digitalWrite(STEPPER_DISABLE, 0); 
          return true;
        case 18:
          Serial.print(F(" Disable")); 
          digitalWrite(STEPPER_DISABLE, 1);
          return true;
        case 91:
          attr = getCommandAttr(results, resultSize, 'Z');
          if(attr != "")
            return setAxisSteps(attr.toInt());
          return true;
        case 100:
          printCommands();
          return true;
        case 114:
          attr = getCommandAttr(results, resultSize, 'Z');
          if(attr != "")
            return setAxisSteps(attr.toInt());
          return false;
      }
      return true;
    
    default:
      Serial.print(F("Unknown command")); 
      return false;
  }  
}

String getCommandAttr(String *attr, int attrSize, char prefix){
  for(int i = 0; i < attrSize; i++){
    if(attr[i].startsWith(prefix)){
      return attr[i].substring(1, attr[i].length());
    }
  }
  return "";
}


int getCommandArray(String *subCommands){
  int e[] = {sBuffer.indexOf(";"), sBuffer.indexOf("\n"), sBuffer.indexOf("\r")};
  int commandEnd = 
    e[0] != -1 ? e[0] : 
    e[1] != -1 ? e[1] :
    e[2];
  String mainCommand = sBuffer.substring(0, commandEnd);
  
  int i = 0, j = 0, stIdx = 0, endIdx = -1;
  while(1){
    endIdx = mainCommand.indexOf(" ", i);
    if(endIdx == -1){
      subCommands[j] = mainCommand.substring(stIdx, mainCommand.length());
      j++;
      break;
    }
    
    subCommands[j] = mainCommand.substring(stIdx, endIdx);
    if(subCommands[j] != " " && subCommands[j] != "")
      j++;
    
    stIdx = endIdx+1;
    
    i++;
  }

  return j;    
}

boolean setAxisSteps(int steps){
  stepsPerUnit = steps;
  setFeedRate(feedRate);
}

// unit(mm)/m
boolean setFeedRate(float _feedRate){
  // feedrate = 100
  // stepsPerUnit * 100 = steps needed to take 
  // steps needed to take / 60000000 = time each step will take aka delay
  feedRate = constrain(_feedRate, MIN_FEEDRATE, MAX_FEEDRATE);

  long stepsPerMin = (long)stepsPerUnit * feedRate;
  stepDelay = 60000000l / constrain(stepsPerMin ,MIN_STEPS_PER_MIN ,MAX_STEPS_PER_MIN);
  
}

boolean startMovement(float pos){
  float stepsToTake = stepsPerUnit * pos; // kinda shitty, probably bad
  stepsToTake = abs(stepsToTake); 
  
  digitalWrite(STEPPER_DIRECTION, pos < 0);
  
  for(int j = 0; j < stepsToTake; j++){
     digitalWrite(STEPPER_STEP, 1);
     delayMicroseconds(500);
     digitalWrite(STEPPER_STEP, 0);
     delayMicroseconds(stepDelay-500);
  }
}
