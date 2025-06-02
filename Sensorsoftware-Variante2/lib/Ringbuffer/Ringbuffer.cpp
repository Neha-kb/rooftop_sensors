#include "Ringbuffer.h"

//Initialisiert den Ringbuffer
void Ringbuffer::CreateRingbuffer(){
    //Initialisiere Variablen
    WritePos = 0;
    ReadPos = 0;

    //Leere Buffer
    for (int i = 0; i < Bufferlength; i++){
        Buffer[i] = 0;
    }
}

//Füge ein Wert dem Ringbuffer hinzu
void Ringbuffer::AddValue(float NewValue){
    Buffer[WritePos] = NewValue;
    NextWritePos();
}

//Springe zur nächsten Schreibposition
void Ringbuffer::NextWritePos(){
    WritePos++;

    if (WritePos >= Bufferlength){
        WritePos=0;
    }

    //Prüfe ob die Schreibpos die Lesepos überrundet 
    if (WritePos == ReadPos)
        NextReadPos();
}

//Springe zur nächsten Schreibposition
void Ringbuffer::NextReadPos(){
    ReadPos++;

    if (ReadPos >= Bufferlength){
        ReadPos=0;
    }
}

// Berechne den Mittelwert aus allen noch nicht gelesenen Werten im Buffer
float Ringbuffer::GetMeanValue(){
    float mean = 0;
    int CountValues = 0;

    //Addiere alle neuen Messwerte auf
    while (ReadPos != WritePos){
        //Addiere Werte auf
        mean = mean + Buffer[ReadPos];
        //Zähle die Messwerte
        CountValues++;
        //Erhöhe die aktuelle Leseposition
        NextReadPos();
    }

    if (CountValues > 0){
        //Berechne den Mittelwert
        mean = mean / CountValues;
    }

    return mean;
}




