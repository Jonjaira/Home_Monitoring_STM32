#ifndef LAUNDRYROOMVIEW_HPP
#define LAUNDRYROOMVIEW_HPP

#include <gui_generated/laundryroom_screen/LaundryRoomViewBase.hpp>
#include <gui/laundryroom_screen/LaundryRoomPresenter.hpp>

class LaundryRoomView : public LaundryRoomViewBase
{
public:
    LaundryRoomView();
    virtual ~LaundryRoomView() {}
    virtual void setupScreen();
    virtual void tearDownScreen();
protected:
};

#endif // LAUNDRYROOMVIEW_HPP
