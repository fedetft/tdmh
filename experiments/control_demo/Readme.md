
# Experiment overview
The experiment aims at controlling the temperature of a __furnace__.

The __sensor__ node reads from a serial port the temperature from the furnace electronic controller and sends it to the master node, on which the controller is running.

The __controller__ takes the new sample and computes the control action, according to the desired temperature set point, forwarding it to the actuator, including the sample original timestamp (that was received together with the sample). It also logs both the received sample and the computed control action.

The __actuator__ node receives the control action and writes it on the serial port, through which the furnace regulates itself. The actuator also computes the end-to-end latency from the moment in which the temperature was sampled (thanks to the timestamp forwarded by the controller). The actuator also computes the latency on the path controller-actuator. Both the computed latencies are sent back to the master node which logs them.

Latency are sent back to the master since the actuator can only output the control action to the only exposed serial port on the WandStem nodes.

```mermaid
stateDiagram-v2
    [*] --> Sensor: Serial port (temperature)
    Sensor --> Controller: Stream (timestamp, temperature)
    Controller --> Actuator: Stream (timestamp, control action)
    Actuator --> Controller: Stream (S-A latency, C-A latency)
    Actuator --> [*]: Serial port (control action)
```

<!--# Controller

As a note, the controller is a __PI controller__ with __anti windup__. The output control action depends on the two previous steps.-->