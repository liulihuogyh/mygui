/*!
	@file
	@author		Albert Semenov
	@date		08/2010
*/

#include "Precompiled.h"
#include "PropertyFloatControl.h"
#include "Localise.h"
#include "FactoryManager.h"

namespace tools
{

	FACTORY_ITEM_ATTRIBUTE(PropertyFloatControl)

	PropertyFloatControl::PropertyFloatControl() :
		mName(nullptr),
		mEdit(nullptr)
	{
	}

	PropertyFloatControl::~PropertyFloatControl()
	{
		mEdit->eventEditTextChange -= MyGUI::newDelegate(this, &PropertyFloatControl::notifyEditTextChange);
	}

	void PropertyFloatControl::OnInitialise(Control* _parent, MyGUI::Widget* _place, const std::string& _layoutName)
	{
		Control::OnInitialise(_parent, _place, "PropertyEditControl.layout");

		assignWidget(mName, "Name", false);
		assignWidget(mEdit, "Edit");

		mEdit->eventEditTextChange += MyGUI::newDelegate(this, &PropertyFloatControl::notifyEditTextChange);
	}

	void PropertyFloatControl::updateCaption()
	{
		Property* proper = getProperty();
		if (proper != nullptr)
			mName->setCaption(proper->getType()->getName());
	}

	void PropertyFloatControl::updateProperty()
	{
		Property* proper = getProperty();
		if (proper != nullptr)
		{
			mEdit->setEnabled(!proper->getType()->getReadOnly());
			if (mEdit->getOnlyText() != proper->getValue())
				mEdit->setCaption(proper->getValue());

			bool validate = isValidate();
			setColour(validate);
		}
		else
		{
			mEdit->setCaption("");
			mEdit->setEnabled(false);
		}
	}

	void PropertyFloatControl::notifyEditTextChange(MyGUI::EditBox* _sender)
	{
		Property* proper = getProperty();
		if (proper != nullptr)
		{
			bool validate = isValidate();
			if (validate)
				executeAction(getClearValue());

			setColour(validate);
		}
	}

	bool PropertyFloatControl::isValidate()
	{
		MyGUI::UString value = mEdit->getOnlyText();

		float value1 = 0;
		if (!MyGUI::utility::parseComplex(value, value1))
			return false;

		return true;
	}

	MyGUI::UString PropertyFloatControl::getClearValue()
	{
		MyGUI::UString value = mEdit->getOnlyText();

		float value1 = 0;
		if (MyGUI::utility::parseComplex(value, value1))
			return MyGUI::utility::toString(value1);

		return "";
	}

	void PropertyFloatControl::setColour(bool _validate)
	{
		MyGUI::UString value = mEdit->getOnlyText();
		if (!_validate)
			value = replaceTags("ColourError") + value;

		size_t index = mEdit->getTextCursor();
		mEdit->setCaption(value);
		mEdit->setTextCursor(index);
	}

}