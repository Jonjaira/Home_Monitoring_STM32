#ifndef LAUNDRYROOMPRESENTER_HPP
#define LAUNDRYROOMPRESENTER_HPP

#include <gui/model/ModelListener.hpp>
#include <mvp/Presenter.hpp>

using namespace touchgfx;

class LaundryRoomView;

class LaundryRoomPresenter : public touchgfx::Presenter, public ModelListener
{
public:
    LaundryRoomPresenter(LaundryRoomView& v);

    /**
     * The activate function is called automatically when this screen is "switched in"
     * (ie. made active). Initialization logic can be placed here.
     */
    virtual void activate();

    /**
     * The deactivate function is called automatically when this screen is "switched out"
     * (ie. made inactive). Teardown functionality can be placed here.
     */
    virtual void deactivate();

    virtual ~LaundryRoomPresenter() {}

private:
    LaundryRoomPresenter();

    LaundryRoomView& view;
};

#endif // LAUNDRYROOMPRESENTER_HPP
