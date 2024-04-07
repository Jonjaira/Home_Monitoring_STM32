#ifndef HOMEVIEW_HPP
#define HOMEVIEW_HPP

#include <gui_generated/home_screen/HomeViewBase.hpp>
#include <gui/home_screen/HomePresenter.hpp>

class HomeView : public HomeViewBase
{
public:
    HomeView();
    virtual ~HomeView() {}
    virtual void setupScreen();
    virtual void tearDownScreen();

    void setBedroomTempHumidity(uint8_t temp, uint8_t humidity);
    void setLivingroomTempHumidity(uint8_t temp, uint8_t humidity);
protected:
};

#endif // HOMEVIEW_HPP
