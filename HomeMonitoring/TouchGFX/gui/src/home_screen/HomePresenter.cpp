#include <gui/home_screen/HomeView.hpp>
#include <gui/home_screen/HomePresenter.hpp>

HomePresenter::HomePresenter(HomeView& v)
    : view(v)
{

}

void HomePresenter::activate()
{

}

void HomePresenter::deactivate()
{

}

void HomePresenter::bedRoomTempHumidityChanged()
{
    view.setBedroomTempHumidity(model->getCurrentBedroomTemp(),
                                model->getCurrentBedroomHumidity());
}

void HomePresenter::livingRoomTempHumidityChanged()
{
    view.setLivingroomTempHumidity(model->getCurrentLivingroomTemp(),
                                   model->getCurrentLivingroomHumidity());
}
