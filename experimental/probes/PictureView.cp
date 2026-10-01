// Probe: TPictureView::ClassID and DerivedFrom, ROM 0x188D38..0x188D74.
// The classes only as far as these two need (the vtables they make are
// not compared).

typedef unsigned char Boolean;

class TView
{
public:
	virtual long	ClassID(void) const;
	virtual Boolean	DerivedFrom(long inClass) const;
};

class TPictureView : public TView
{
public:
	virtual long	ClassID(void) const;
	virtual Boolean	DerivedFrom(long inClass) const;
};

long
TPictureView::ClassID(void) const
{
	return 76;		// clPictureView
}

Boolean
TPictureView::DerivedFrom(long inClass) const
{
	return inClass == 76 || TView::DerivedFrom(inClass);
}
