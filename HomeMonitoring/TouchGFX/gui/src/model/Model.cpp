#include <gui/model/Model.hpp>
#include <gui/model/ModelListener.hpp>

#include <memory.h>
#include <cmsis_os2.h>

using json = nlohmann::json;

extern "C"
{
    extern osMessageQueueId_t SensorsQueueHandle;
    extern uint8_t buffer[128];
}

Model::Model() : modelListener(0)
{

}

void Model::tick()
{
    uint8_t event = 0;

    if (osMessageQueueGet(SensorsQueueHandle, &event, 0U, 0) == osOK)
    {
        sensorData = std::string(buffer, buffer + strlen(reinterpret_cast < const char* >(buffer)));

        auto j = json::parse(sensorData);

        // Iterate through the top-level objects (rooms)
        for (auto& [room, sensor_data] : j.items())
        {
            // Determine the sensor type
            sensorType = sensor_data.begin().key();
            roomPlacement = room;

            // Temperature and Humidity Sensor
            if (sensorType == "TempSensor")
            {
                uint8_t temperature = sensor_data[sensorType]["Temperature"];
                uint8_t humidity = sensor_data[sensorType]["Humidity"];

                if (roomPlacement == "BedRoom")
                {
                    currentBedRoomTemp = temperature;
                    currentBedRoomHumidity = humidity;
                    modelListener->bedRoomTempHumidityChanged();
                }
                else if (roomPlacement == "LivingRoom")
                {
                    currentLivingRoomTemp = temperature;
                    currentLivingRoomHumidity = humidity;
                    modelListener->livingRoomTempHumidityChanged();
                }
                else if (roomPlacement == "LaundryRoom")
                {
                    currentLaundryRoomTemp = temperature;
                    currentLaundryRoomHumidity = humidity;
                }
                else if (roomPlacement == "Garage")
                {
                    currentGarageTemp = temperature;
                    currentGarageHumidity = humidity;
                }
                else
                {

                }
            }
            // Motion Sensor
            else if (sensorType == "MotionSensor")
            {
                bool motion = sensor_data[sensorType]["Motion"] == "true";

                if (roomPlacement == "BedRoom")
                {
                    isMotionInTheBedRoom = motion;
                }
                else if (roomPlacement == "LivingRoom")
                {
                    isMotionInTheLivingRoom = motion;
                }
                else if (roomPlacement == "LaundryRoom")
                {
                    isMotionInTheLaundryRoom = motion;
                }
                else if (roomPlacement == "Garage")
                {
                    isMotionInTheGarage = motion;
                }
                else
                {

                }
            }
            else
            {

            }
        }
    }
}
