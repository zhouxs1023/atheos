#include <gui/view.h>
#include <util/invoker.h>
#include <storage/path.h>

#include <string>
#include <vector>

using namespace os;

class IconView;
class BitmapView;

class Icon
{
public:
    Icon( const char* pzTitle, const char* pzPath, const struct stat& sStat );
    ~Icon();
    const std::string& GetName() const { return( m_cTitle ); }
    Rect 		     GetBounds( Font* pcFont );
    Rect 		     GetFrame( Font* pcFont );
    void 		     Paint( View* pcView, const Point& cOffset, bool bLarge, bool bBlendText );
    void 		     Select( BitmapView* pcView, bool bSelected );
    void 		     Select( IconView* pcView, bool bSelected );
    bool		     IsSelected() const { return( m_bSelected ); }
    Bitmap* GetBitmap();
  
    Point	m_cPosition;
    bool		m_bSelected;
    struct stat	m_sStat;
private:
    float GetStrWidth( Font* pcFont );
    static Bitmap* s_pcBitmap[16];
    static int s_nCurBitmap;
    Rect	m_cBounds;
    float	m_vStringWidth;
    int		m_nMaxStrLen;
    bool	m_bBoundsValid;
    bool	m_bStrWidthValid;
    std::string	m_cTitle;
    uint8	m_anSmall[16*16*4];
    uint8	m_anLarge[32*32*4];
};

class IconView : public View, public Invoker
{
public:
  IconView( const Rect& cFrame, const char* pzPath, Bitmap* pcBitmap = NULL );
  ~IconView();

  void	       LayoutIcons();
  virtual void SetDirChangeMsg( Message* pcMsg );
  virtual void Invoked();
  virtual void DirChanged( const std::string& cNewPath );
  
  void	      SetPath( const std::string& cPath );
  std::string GetPath();
  
  Icon*		FindIcon( const Point& cPos );
  void 		Erase( const Rect& cFrame );
  
  virtual void	AttachedToWindow();
  virtual void  KeyDown( const char* pzString, const char* pzRawString, uint32 nQualifiers );
  virtual void	MouseDown( const Point& cPosition, uint32 nButtons );
  virtual void	MouseUp( const Point& cPosition, uint32 nButtons, Message* pcData );
  virtual void	MouseMove( const Point& cNewPos, int nCode, uint32 nButtons, Message* pcData );
  virtual void	Paint( const Rect& cUpdateRect );
private:
  static int32 ReadDirectory( void* pData );
  void ReRead();
  
  struct ReadDirParam {
    ReadDirParam( IconView* pcView ) { m_pcView = pcView; m_bCancel = false; }
    IconView* m_pcView;
    bool      m_bCancel;
  };
  Path		     m_cPath;
  ReadDirParam*	     m_pcCurReadDirSession;

  Message*	     m_pcDirChangeMsg;
  Point	     	     m_cLastPos;
  Point	     	     m_cDragStartPos;
  Rect		     m_cSelRect;
  bigtime_t	     m_nHitTime;
  Bitmap*	     m_pcBitmap;
  std::vector<Icon*> m_cIcons;
  bool		     m_bCanDrag;
  bool		     m_bSelRectActive;
};
