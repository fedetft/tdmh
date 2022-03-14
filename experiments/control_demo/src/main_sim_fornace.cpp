
#include <stdio.h>
#include <algorithm>
#include <cstdlib>
#include <miosix.h>

using namespace std;
using namespace miosix;

float mu = 1250.f;
float T  = 200.f;
float Ts = 1.f;
float a  = 0.9985f;

float start_temp = 20.f;

float u     = 0.f;        // control action
float y_old = start_temp; // system output (temperature) at previous step
float y     = y_old;      // system output (temperature)

bool newTempComputed = false;

// Output sensor (temperature) data at 10 Hz
void outputSensorData(void *arg) {
    float t = start_temp;
    while(true) {

        if (newTempComputed) {
            t = y;
            newTempComputed = false;
        }
        
        printf("s:%d\n", static_cast<int>(t));
        Thread::sleep(100);
    }
}

// Execute a single process step
float processStep(float u_k, float y_k_1) {
    return (T / (T + Ts)) * y_k_1 + mu * (Ts / (T + Ts)) * u_k;
}

int main()
{
    Thread::create(&outputSensorData, 2048, MAIN_PRIORITY, nullptr);

    int cv = 0;

    while (true)
    {
        // Read control action from serial
        while(scanf("cv:%d", &cv) != 1) { // consume input until finally the scanf only reads 1 value
            while(fgetc(stdin) != '\n'); // if it is not 1, consume the entire line
        }

         // Real control action has 60k steps, but "u" goes from 0 to 1
        u = static_cast<float>(cv) / 60000.f;
        y = processStep(u, y_old);
        newTempComputed = true;

        y_old = y;

        // ledOn();
        // Thread::sleep(50);
        // ledOff();
        // Thread::sleep(50);
    }

    return 0;
}