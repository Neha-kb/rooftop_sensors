#ifndef RINGBUFFER_H
#define RINGBUFFER_H
class Ringbuffer{
private:
    int WritePos;
    int ReadPos;
    #define Bufferlength 300
    float Buffer[Bufferlength];
    void NextWritePos();
    void NextReadPos();

public: 
    void CreateRingbuffer();
    void AddValue(float NewValue);
    float GetMeanValue();
};

#endif