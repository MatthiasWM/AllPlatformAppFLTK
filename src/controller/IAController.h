//
//  IAController.h
//
//  Copyright (c) 2013-2018 Matthias Melcher. All rights reserved.
//

#ifndef IA_CONTROLLER_H
#define IA_CONTROLLER_H


#include <vector>


class IACtrlTreeItemFloat;
class IAPropertyFloat;


class IAController
{
public:
    IAController();
    virtual ~IAController();

    virtual void propertyValueChanged();
};


/**
 * This controller manages the connection between a Floating Point Property
 * and a Floating Point Tree View.
 */
class IACtrlTreeItemFloat : public IAController
{
public:
    IACtrlTreeItemFloat();
    virtual ~IACtrlTreeItemFloat() override;

    virtual void propertyValueChanged() override;

protected:
    IAPropertyFloat *pProperty = nullptr;
    IACtrlTreeItemFloat *pView = nullptr;
};


#endif /* IA_PRINTER_LIST_CONTROLLER_H */

