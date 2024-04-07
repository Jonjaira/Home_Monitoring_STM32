#ifndef GARAGEVIEW_HPP
#define GARAGEVIEW_HPP

#include <gui_generated/garage_screen/GarageViewBase.hpp>
#include <gui/garage_screen/GaragePresenter.hpp>

class GarageView : public GarageViewBase
{
public:
    GarageView();
    virtual ~GarageView() {}
    virtual void setupScreen();
    virtual void tearDownScreen();
protected:
};

#endif // GARAGEVIEW_HPP
