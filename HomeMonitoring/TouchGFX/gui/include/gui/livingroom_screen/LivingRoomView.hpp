#ifndef LIVINGROOMVIEW_HPP
#define LIVINGROOMVIEW_HPP

#include <gui_generated/livingroom_screen/LivingRoomViewBase.hpp>
#include <gui/livingroom_screen/LivingRoomPresenter.hpp>

class LivingRoomView : public LivingRoomViewBase
{
public:
    LivingRoomView();
    virtual ~LivingRoomView() {}
    virtual void setupScreen();
    virtual void tearDownScreen();
protected:
};

#endif // LIVINGROOMVIEW_HPP
