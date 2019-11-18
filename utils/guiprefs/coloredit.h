#include <gui/view.h>

namespace os {
  class Spinner;
  class StringView;
}
using namespace os;

class ColorEdit : public View
{
public:
    ColorEdit( const Rect& cFrame, const char* pzName, const Color32_s& sColor );
    void	     SetValue( const Color32_s& sColor );
    const Color32_s& GetValue() const;

    virtual void  AllAttached();
    virtual void  FrameSized( const Point& cDelta );
    virtual Point GetPreferredSize( bool bLargest ) const;
    virtual void  HandleMessage( Message* pcMessage );
    virtual void  Paint( const Rect& cUpdateRect );
  
private:
    Spinner*	m_pcRedSpinner;
    StringView*	m_pcRedStr;
    Spinner*	m_pcGreenSpinner;
    StringView*	m_pcGreenStr;
    Spinner*	m_pcBlueSpinner;
    StringView*	m_pcBlueStr;
    Rect		m_cTestRect;
  
    Color32_s	m_sColor;
};
