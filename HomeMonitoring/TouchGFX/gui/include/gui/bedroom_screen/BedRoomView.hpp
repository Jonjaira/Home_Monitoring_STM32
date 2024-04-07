#ifndef BEDROOMVIEW_HPP
#define BEDROOMVIEW_HPP

#include <gui_generated/bedroom_screen/BedRoomViewBase.hpp>
#include <gui/bedroom_screen/BedRoomPresenter.hpp>

class BedRoomView : public BedRoomViewBase
{
public:
    BedRoomView();
    virtual ~BedRoomView() {}
    virtual void setupScreen();
    virtual void tearDownScreen();
protected:
};

#endif // BEDROOMVIEW_HPP
