#ifndef LIVINGROOMPRESENTER_HPP
#define LIVINGROOMPRESENTER_HPP

#include <gui/model/ModelListener.hpp>
#include <mvp/Presenter.hpp>

using namespace touchgfx;

class LivingRoomView;

class LivingRoomPresenter : public touchgfx::Presenter, public ModelListener
{
public:
    LivingRoomPresenter(LivingRoomView& v);

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

    virtual ~LivingRoomPresenter() {}

private:
    LivingRoomPresenter();

    LivingRoomView& view;
};

#endif // LIVINGROOMPRESENTER_HPP
