#ifndef DUMMY_MOTORS_H
#define DUMMY_MOTORS_H

#include "Arduino.h"

#define sgn(x) (((x) > 0) - ((x) < 0))

class DummyMotor {
    public:
    DummyMotor()
    {
        this->maxSpeed = 0;
        this->pos = 0;
        this->acc = 0;
        this->speed = 0;
        this->targetPos = 0;
        this->lastUpdate = millis()/1000.0;
        this->slowingDown = false;
        this->moving = false;
    }

    void setMaxSpeed(double s)
    {
        this->maxSpeed = s;
    }
    void setPosition(double pos)
    {
        this->pos = pos;
    }

    void setAcceleration(double acc)
    {
        this->acc = acc;
    }
    double getPosition()
    {
        return this->pos;
    }
    double moveTo(double pos)
    {
        this->targetPos = pos;
        this->lastUpdate = millis()/1000.0;
        this->speed = 0;
        this->moving = true;
        this->slowingDown = false;
    }

    bool isMoving()
    {
        return this->moving;
    }

    void run()
    {
        double deltaT = abs(millis()/1000.0 - this->lastUpdate);
        this->lastUpdate = millis()/1000.0;
        if(this->moving)
        { 
            // calculate the distance required to slow down at this speed
            double time_to_slow_down = this->speed / this->acc;
            double distance_to_slow_down = this->speed * time_to_slow_down - 0.5 * this->acc * time_to_slow_down * time_to_slow_down;

            // apply the acceleration in an almost leapfrog manner
            if(abs(this->targetPos - this->pos) > distance_to_slow_down && !this->slowingDown)
            {
                if(this->speed + 0.5 * this->acc * deltaT < this->maxSpeed)
                {
                    this->speed += 0.5 * this->acc * deltaT;
                }

                if(abs(this->targetPos - this->pos) > 0.001)
                {
                    this->pos += this->speed * deltaT * sgn(this->targetPos - this->pos);
                }
                

                if(this->speed + 0.5 * this->acc * deltaT < this->maxSpeed)
                {
                    this->speed += 0.5 * this->acc * deltaT;
                }
            }
            else
            {
                this->slowingDown = true;
                if(this->speed - 0.5 * this->acc * deltaT > 0)
                {
                    this->speed -= 0.5 * this->acc * deltaT;
                }

                if(abs(this->targetPos - this->pos) > 0.001)
                {
                    this->pos += this->speed * deltaT * sgn(this->targetPos - this->pos);
                }

                if(this->speed - 0.5 * this->acc * deltaT > 0)
                {
                    this->speed -= 0.5 * this->acc * deltaT;
                }
            }

            if(abs(this->targetPos - this->pos) < 0.001)
            {
                this->slowingDown = false;
                this->moving = false;
                this->pos = this->targetPos;
            }
        }
    }

    private:
    double maxSpeed;
    double pos;
    double acc;
    double speed;
    double targetPos;
    double lastUpdate;
    bool slowingDown;
    bool moving;
};

#endif