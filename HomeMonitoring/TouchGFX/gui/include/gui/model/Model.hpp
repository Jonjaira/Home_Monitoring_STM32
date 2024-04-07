#ifndef MODEL_HPP
#define MODEL_HPP

#include <stdint.h>
#include <string>

#include "json.hpp"

class ModelListener;

class Model
{
public:
    Model();

    void bind(ModelListener* listener)
    {
        modelListener = listener;
    }

    void tick();

    uint8_t getCurrentBedroomTemp() { return currentBedRoomTemp; }
    void setCurrentBedroomTemp(uint8_t temp) { currentBedRoomTemp = temp; }
    uint8_t getCurrentBedroomHumidity() { return currentBedRoomHumidity; }
    void setCurrentBedroomHumidity(uint8_t humidity) { currentBedRoomHumidity = humidity; }
    bool isBedroomMotion() { return isMotionInTheBedRoom; }
    void setBedroomMotion(bool motion) { isMotionInTheBedRoom = motion; }

    uint8_t getCurrentLivingroomTemp() { return currentLivingRoomTemp; }
    void setCurrentLivingroomTemp(uint8_t temp) { currentLivingRoomTemp = temp; }
    uint8_t getCurrentLivingroomHumidity() { return currentLivingRoomHumidity; }
    void setCurrentLivingroomHumidity(uint8_t humidity) { currentLivingRoomHumidity = humidity; }
    bool isLivingroomMotion() { return isMotionInTheLivingRoom; }
    void setLivingroomMotion(bool motion) { isMotionInTheLivingRoom = motion; }

    uint8_t getCurrentLaundryroomTemp() { return currentLaundryRoomTemp; }
    void setCurrentLaundryroomTemp(uint8_t temp) { currentLaundryRoomTemp = temp; }
    uint8_t getCurrentLaundryroomHumidity() { return currentLaundryRoomHumidity; }
    void setCurrentLaundryroomHumidity(uint8_t humidity) { currentLaundryRoomHumidity = humidity; }
    bool isLaundryroomMotion() { return isMotionInTheLaundryRoom; }
    void setLaundryroomMotion(bool motion) { isMotionInTheLaundryRoom = motion; }

    uint8_t getCurrentGarageTemp() { return currentGarageTemp; }
    void setCurrentGarageTemp(uint8_t temp) { currentGarageTemp = temp; }
    uint8_t getCurrentGarageHumidity() { return currentGarageHumidity; }
    void setCurrentGarageHumidity(uint8_t humidity) { currentGarageHumidity = humidity; }
    bool isGarageMotion() { return isMotionInTheGarage; }
    void setGarageMotion(bool motion) { isMotionInTheGarage = motion; }


protected:
    ModelListener* modelListener;

    std::string roomPlacement;
    std::string sensorType;

    uint8_t currentBedRoomTemp;
    uint8_t currentBedRoomHumidity;
    bool    isMotionInTheBedRoom;

    uint8_t currentLivingRoomTemp;
    uint8_t currentLivingRoomHumidity;
    bool    isMotionInTheLivingRoom;

    uint8_t currentLaundryRoomTemp;
    uint8_t currentLaundryRoomHumidity;
    bool    isMotionInTheLaundryRoom;

    uint8_t currentGarageTemp;
    uint8_t currentGarageHumidity;
    bool    isMotionInTheGarage;

    std::string sensorData;
};

#endif // MODEL_HPP
