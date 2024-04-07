#ifndef GARAGEPRESENTER_HPP
#define GARAGEPRESENTER_HPP

#include <gui/model/ModelListener.hpp>
#include <mvp/Presenter.hpp>

using namespace touchgfx;

class GarageView;

class GaragePresenter : public touchgfx::Presenter, public ModelListener
{
public:
    GaragePresenter(GarageView& v);

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

    virtual ~GaragePresenter() {}

private:
    GaragePresenter();

    GarageView& view;
};

#endif // GARAGEPRESENTER_HPP
