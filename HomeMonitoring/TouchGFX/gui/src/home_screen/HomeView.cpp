#include <gui/home_screen/HomeView.hpp>

HomeView::HomeView()
{

}

void HomeView::setupScreen()
{
    HomeViewBase::setupScreen();
}

void HomeView::tearDownScreen()
{
    HomeViewBase::tearDownScreen();
}

void HomeView::setBedroomTempHumidity(uint8_t temp, uint8_t humidity)
{
    Unicode::itoa(temp, BedRoomTempValBuffer, BEDROOMTEMPVAL_SIZE, 10);
    BedRoomTempVal.invalidate();

    Unicode::itoa(humidity, BedRoomHumidityValBuffer, BEDROOMHUMIDITYVAL_SIZE, 10);
    BedRoomHumidityVal.invalidate();
}

void HomeView::setLivingroomTempHumidity(uint8_t temp, uint8_t humidity)
{
    Unicode::itoa(temp, LivinRoomTempValBuffer, LIVINROOMTEMPVAL_SIZE, 10);
    BedRoomTempVal.invalidate();

    Unicode::itoa(humidity, LivinRoomHumidityValBuffer, LIVINROOMTEMPVAL_SIZE, 10);
    BedRoomHumidityVal.invalidate();
}
