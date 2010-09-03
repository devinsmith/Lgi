#include <ctype.h>
#include "GCss.h"
#include "LgiCommon.h"

#define SkipWhite(s) 	while (*s && strchr(WhiteSpace, *s)) s++;

#define IsNumeric(s)	((*s) && (strchr("-.", *s) || isdigit(*s)))

#define CopyPropOnSave(Type, Id) \
	{ \
		Type *e = (Type*)Props.Find(Id); \
		if (e) *e = *t; \
		else Props.Add(Id, new Type(*t)); \
	}
#define ReleasePropOnSave(Type, Id) \
	{ \
		Type *e = (Type*)Props.Find(Id); \
		if (e) *e = *t; \
		else Props.Add(Id, t.Release()); \
	}


GHashTbl<char*, GCss::PropType> GCss::Lut(0, false);
char *GCss::PropName(PropType p)
{
	char *s;
	for (PropType t = Lut.First(&s); t; t = Lut.Next(&s))
	{
		if (p == t)
			return s;
	}

	LgiAssert(!"Add this field to the LUT");
	return 0;
}

/////////////////////////////////////////////////////////////////////////////
bool ParseWord(char *&s, char *word)
{
	char *doc = s;
	while (*doc && *word)
	{
		if (tolower(*doc) == tolower(*word))
		{
			doc++;
			word++;
		}
		else return false;
	}
	
	if (*word)
		return false;

	if (*doc && (isalpha(*doc) || isdigit(*doc)))
		return false;

	s = doc;
	return true;
}

bool ParseProp(char *&s, char *w)
{
	char *doc = s;
	char *word = w;
	while (*doc && *word)
	{
		if (tolower(*doc) == tolower(*word))
		{
			doc++;
			word++;
		}
		else return false;
	}
	
	if (*word)
		return false;

	SkipWhite(doc);
	if (*doc != ':')
		return false;

	s = doc + 1;
	return true;
}

int ParseComponent(char *&s)
{
	int ret = 0;

	SkipWhite(s);

	if (!strnicmp(s, "0x", 2))
	{
		ret = htoi(s);
		while (*s && (isdigit(*s) || *s == 'x' || *s == 'X')) s++;
	}
	else
	{
		ret = atoi(s);
		while (*s && (isdigit(*s) || *s == '.' || *s == '-')) s++;

		SkipWhite(s);
		if (*s == '%')
		{
			s++;
			ret = ret * 255 / 100;
		}
	}

	SkipWhite(s);

	return ret;
}

char *ParseString(char *&s)
{
	char *ret = 0;

	if (*s == '\'' || *s == '\"')
	{
		char delim = *s++;
		char *e = strchr(s, delim);
		if (!e)
			return 0;

		ret = NewStr(s, e - s);
		s = e + 1;
	}
	else
	{
		char *e = s;
		while (*e && (isalpha(*e) || isdigit(*e) || strchr("_-", *e)))
			e++;

		ret = NewStr(s, e - s);
		s = e;
	}

	return ret;
}

/////////////////////////////////////////////////////////////////////////////
GCss::GCss() : Props(0, false, PropNull)
{
	if (Lut.Length() == 0)
	{
		Lut.Add("border-collapse", PropBorderCollapse);
		Lut.Add("letter-spacing", PropLetterSpacing);
		Lut.Add("list-style-type", PropListStyleType);
		Lut.Add("word-wrap", PropWordWrap);
		Lut.Add("text-align", PropTextAlign);
		Lut.Add("text-decoration", PropTextDecoration);
		Lut.Add("display", PropDisplay);
		Lut.Add("float", PropFloat);
		Lut.Add("position", PropPosition);
		Lut.Add("overflow", PropOverflow);
		Lut.Add("visibility", PropVisibility);
		Lut.Add("font-size", PropFontSize);
		Lut.Add("font-style", PropFontStyle);
		Lut.Add("font-variant", PropFontVariant);
		Lut.Add("font-weight", PropFontWeight);
		Lut.Add("background", PropBackgroundColor);
		Lut.Add("background-color", PropBackgroundColor);
		Lut.Add("background-image", PropBackgroundImage);
		Lut.Add("background-repeat", PropBackgroundRepeat);
		Lut.Add("background-attachment", PropBackgroundAttachment);
		Lut.Add("z-index", PropZIndex);
		Lut.Add("width", PropWidth);
		Lut.Add("min-width", PropMinWidth);
		Lut.Add("max-width", PropMaxWidth);
		Lut.Add("height", PropHeight);
		Lut.Add("min-height", PropMinHeight);
		Lut.Add("max-height", PropMaxHeight);
		Lut.Add("top", PropTop);
		Lut.Add("right", PropRight);
		Lut.Add("bottom", PropBottom);
		Lut.Add("left", PropLeft);
		Lut.Add("margin", PropMargin);
		Lut.Add("margin-top", PropMarginTop);
		Lut.Add("margin-right", PropMarginRight);
		Lut.Add("margin-bottom", PropMarginBottom);
		Lut.Add("margin-left", PropMarginLeft);
		Lut.Add("padding", PropPadding);
		Lut.Add("padding-top", PropPaddingTop);
		Lut.Add("padding-right", PropPaddingRight);
		Lut.Add("padding-bottom", PropPaddingBottom);
		Lut.Add("padding-left", PropPaddingLeft);
		Lut.Add("border", PropBorder);
		Lut.Add("border-top", PropBorderTop);
		Lut.Add("border-right", PropBorderRight);
		Lut.Add("border-bottom", PropBorderBottom);
		Lut.Add("border-left", PropBorderLeft);
		Lut.Add("line-height", PropLineHeight);
		Lut.Add("vertical-align", PropVerticalAlign);
		Lut.Add("fontsize", PropFontSize);
		Lut.Add("background-x", PropBackgroundX);
		Lut.Add("background-y", PropBackgroundY);
		Lut.Add("clip", PropClip);
		Lut.Add("color", PropColor);
		Lut.Add("font-family", PropFontFamily);
	}
}

GCss::GCss(const GCss &c) : Props(0, false, PropNull)
{
	*this = c;
}

GCss::~GCss()
{
	Empty();
}

bool GCss::Len::ToString(GStream &p)
{
	char *Unit = 0;
	switch (Type)
	{
		case LenPx: Unit = "px"; break;
		case LenPt: Unit = "pt"; break;
		case LenEm: Unit = "em"; break;
		case LenEx: Unit = "ex"; break;
		case LenPercent: Unit = "%"; break;
	}

	if (Unit)
	{
		p.Print("%g%s", Value, Unit);
	}
	else
	{
		switch (Type)
		{
			case LenInherit: Unit = "Inherit"; break;
			case LenAuto: Unit = "Auto"; break;
			case LenNormal: Unit = "Normal"; break;
			default: return false;
		}

		if (Unit)
		{
			p.Print("%s", Unit);
		}
		else
		{
			LgiAssert(!"Impl me.");
			return false;
		}
	}

	return true;
}

bool GCss::ColorDef::ToString(GStream &p)
{
	switch (Type)
	{
		case ColorRgb:
		{
			p.Print("#%02.2x%02.2x%02.2x", R32(Rgb32), G32(Rgb32), B32(Rgb32));
			break;
		}
		default:
		{
			LgiAssert(!"Impl me.");
			return false;
		}
	}

	return true;
}

GAutoString GCss::ToString()
{
	GStringPipe p;

	PropType Prop;
	for (void *v = Props.First((int*)&Prop); v; v = Props.Next((int*)&Prop))
	{
		switch (Prop >> 8)
		{
			case TypeEnum:
			{
				char *s = 0;
				char *Name = PropName(Prop);
				switch (Prop)
				{
					case PropFontWeight:
					{
						FontWeightType *b = (FontWeightType*)v;
						switch (*b)
						{
							case FontWeightInherit: s = "inherit"; break;
							case FontWeightNormal: s = "normal"; break;
							case FontWeightBold: s = "bold"; break;
							case FontWeightBolder: s = "bolder"; break;
							case FontWeightLighter: s = "lighter"; break;
							case FontWeight100: s = "100"; break;
							case FontWeight200: s = "200"; break;
							case FontWeight300: s = "300"; break;
							case FontWeight400: s = "400"; break;
							case FontWeight500: s = "500"; break;
							case FontWeight600: s = "600"; break;
							case FontWeight700: s = "700"; break;
							case FontWeight800: s = "800"; break;
							case FontWeight900: s = "900"; break;
						}
						break;
					}
					case PropFontStyle:
					{
						FontStyleType *t = (FontStyleType*)v;
						switch (*t)
						{
							case FontStyleInherit: s = "inherit"; break;
							case FontStyleNormal: s = "normal"; break;
							case FontStyleItalic: s = "italic"; break;
							case FontStyleOblique: s = "oblique"; break;
						}
						break;
					}
					case PropTextDecoration:
					{
						TextDecorType *d = (TextDecorType*)v;
						switch (*d)
						{
							case TextDecorInherit: s= "inherit"; break;
							case TextDecorNone: s= "none"; break;
							case TextDecorUnderline: s= "underline"; break;
							case TextDecorOverline: s= "overline"; break;
							case TextDecorLineThrough: s= "line-Through"; break;
							case TextDecorBlink: s= "blink"; break;
						}
						break;
					}
					case PropDisplay:
					{
						DisplayType *d = (DisplayType*)v;
						switch (*d)
						{
							case DispInherit: s = "inherit"; break;
							case DispBlock: s = "block"; break;
							case DispInline: s = "inline"; break;
							case DispInlineBlock: s = "inline-block"; break;
							case DispListItem: s = "list-item"; break;
							case DispNone: s = "none"; break;
						}
						break;
					}
					case PropFloat:
					{
						FloatType *d = (FloatType*)v;
						switch (*d)
						{
							case FloatInherit: s = "inherit"; break;
							case FloatLeft: s = "left"; break;
							case FloatRight: s = "right"; break;
							case FloatNone: s = "none"; break;
						}
						break;
					}
					case PropPosition:
					{
						PositionType *d = (PositionType*)v;
						switch (*d)
						{
							case PosInherit: s = "inherit"; break;
							case PosStatic: s = "static"; break;
							case PosRelative: s = "relative"; break;
							case PosAbsolute: s = "absolute"; break;
							case PosFixed: s = "fixed"; break;
						}
						break;
					}
					case PropOverflow:
					{
						OverflowType *d = (OverflowType*)v;
						switch (*d)
						{
							case OverflowInherit: s = "inherit"; break;
							case OverflowVisible: s = "visible"; break;
							case OverflowHidden: s = "hidden"; break;
							case OverflowScroll: s = "scroll"; break;
							case OverflowAuto: s = "auto"; break;
						}
						break;
					}
					case PropVisibility:
					{
						VisibilityType *d = (VisibilityType*)v;
						switch (*d)
						{
							case VisibilityInherit: s = "inherit"; break;
							case VisibilityVisible: s = "visible"; break;
							case VisibilityHidden: s = "hidden"; break;
							case VisibilityCollapse: s = "collapse"; break;
						}
						break;
					}
					case PropFontVariant:
					{
						FontVariantType *d = (FontVariantType*)v;
						switch (*d)
						{
							case FontVariantInherit: s = "inherit"; break;
							case FontVariantNormal: s = "normal"; break;
							case FontVariantSmallCaps: s = "small-caps"; break;
						}
						break;
					}
					case PropBackgroundRepeat:
					{
						RepeatType *d = (RepeatType*)v;
						switch (*d)
						{
							case RepeatInherit: s = "inherit"; break;
							case RepeatBoth: s = "repeat"; break;
							case RepeatX: s = "repeat-x"; break;
							case RepeatY: s = "repeat-y"; break;
							case RepeatNone: s = "none"; break;
						}
						break;
					}
					case PropBackgroundAttachment:
					{
						AttachmentType *d = (AttachmentType*)v;
						switch (*d)
						{
							case AttachmentInherit: s = "inherit"; break;
							case AttachmentScroll: s = "scroll"; break;
							case AttachmentFixed: s = "fixed"; break;
						}
						break;
					}
					default:
					{
						LgiAssert(!"Impl me.");
						break;
					}
				}
				
				if (s) p.Print("%s:%s;", Name, s);
				break;
			}
			case TypeLen:
			{
				Len *l = (Len*)v;
				char *Name = PropName(Prop);
				p.Print("%s:", Name);
				l->ToString(p);
				p.Print(";");
				break;
			}
			case TypeGRect:
			{
				GRect *r = (GRect*)v;
				char *Name = PropName(Prop);
				p.Print("%s:rect(%ipx,%ipx,%ipx,%ipx);", Name, r->y1, r->x2, r->y2, r->x1);
				break;
			}
			case TypeColor:
			{
				ColorDef *c = (ColorDef*)v;
				char *Name = PropName(Prop);
				p.Print("%s:", Name);
				c->ToString(p);
				p.Print(";");
				break;
			}
			case TypeImage:
			{
				ImageDef *i = (ImageDef*)v;
				char *Name = PropName(Prop);
				switch (i->Type)
				{
					case ImageInherit:
					{
						p.Print("%s:inherit;", Name);
						break;
					}
					case ImageOwn:
					case ImageRef:
					{
						if (i->Ref)
							p.Print("%s:url(%s);", Name, i->Ref.Get());
						break;
					}
				}
				break;
			}
			case TypeBorder:
			{
				BorderDef *b = (BorderDef*)v;
				char *Name = PropName(Prop);

				p.Print("%s:", Name);
				b->ToString(p);

				char *s = 0;
				switch (b->Style)
				{
					case BorderNone: s = "none"; break;
					case BorderHidden: s = "hidden"; break;
					case BorderDotted: s = "dotted"; break;
					case BorderDashed: s = "dashed"; break;
					case BorderDouble: s = "double"; break;
					case BorderGroove: s = "groove"; break;
					case BorderRidge: s = "ridge"; break;
					case BorderInset: s = "inset"; break;
					case BorderOutset: s = "outset"; break;
					default:
					case BorderSolid: s = "solid"; break;
				}
				p.Print(" %s ", s);
				b->Color.ToString(p);
				p.Print(";");
				break;
			}
			case TypeStrings:
			{
				StringsDef *s = (StringsDef*)v;
				char *Name = PropName(Prop);
				p.Print("%s:", Name);
				for (int i=0; i<s->Length(); i++)
				{
					p.Print("%s%s", i?",":"", (*s)[i]);
				}
				p.Print(";");
				break;
			}
			default:
			{
				LgiAssert(!"Invalid type.");
				break;
			}
		}
	}

	return GAutoString(p.NewStr());
}

bool GCss::ApplyInherit(GCss &c, GArray<PropType> *Types)
{
	int StillInherit = 0;

	if (Types)
	{
		for (int i=0; i<Types->Length(); i++)
		{
			PropType p = (*Types)[i];
			switch (p >> 8)
			{
				#define InheritEnum(prop, type, inherit) \
					case prop: \
					{ \
						type *Mine = (type*)Props.Find(p); \
						if (!Mine || *Mine == inherit) \
						{ \
							type *Theirs = (type*)c.Props.Find(p); \
							if (Theirs) \
							{ \
								if (!Mine) Props.Add(p, Mine = new type); \
								*Mine = *Theirs; \
							} \
							else StillInherit++; \
						} \
						break; \
					}

				#define InheritClass(prop, type, inherit) \
					case prop: \
					{ \
						type *Mine = (type*)Props.Find(p); \
						if (!Mine || Mine->Type == inherit) \
						{ \
							type *Theirs = (type*)c.Props.Find(p); \
							if (Theirs) \
							{ \
								if (!Mine) Props.Add(p, Mine = new type); \
								*Mine = *Theirs; \
							} \
							else StillInherit++; \
						} \
						break; \
					}



				case TypeEnum:
				{
					switch (p)
					{
						InheritEnum(PropFontStyle, FontStyleType, FontStyleInherit);
						InheritEnum(PropFontVariant, FontVariantType, FontVariantInherit);
						InheritEnum(PropFontWeight, FontWeightType, FontWeightInherit);
						InheritEnum(PropTextDecoration, TextDecorType, TextDecorInherit);
						default:
						{
							LgiAssert(!"Not impl.");
							break;
						}
					}
					break;
				}
				case TypeLen:
				{
					Len *Mine = (Len*)Props.Find(p);
					if (!Mine || Mine->Type == LenInherit || Mine->Type == LenPercent)
					{
						Len *Theirs = (Len*)c.Props.Find(p);
						if (Theirs && Theirs->Type != LenInherit)
						{
							if (!Mine) Props.Add(p, Mine = new Len);
							if (Mine->Type == LenPercent)
							{
								Mine->Type = Theirs->Type;
								Mine->Value = Theirs->Value * Mine->Value / 100.0;
							}
							else if (Mine->Type == LenEm)
							{
								LgiAssert(!"Impl me.");
							}
							else
							{
								*Mine = *Theirs;
							}
						}
						else StillInherit++;
					}
					break;
				}
				case TypeGRect:
				{
					GRect *Mine = (GRect*)Props.Find(p);
					if (!Mine || !Mine->Valid())
					{
						GRect *Theirs = (GRect*)c.Props.Find(p);
						if (Theirs)
						{
							if (!Mine) Props.Add(p, Mine = new GRect);
							*Mine = *Theirs;
						}
						else StillInherit++;
					}
					break;
				}
				case TypeStrings:
				{
					StringsDef *Mine = (StringsDef*)Props.Find(p);
					if (!Mine || Mine->Length() == 0)
					{
						StringsDef *Theirs = (StringsDef*)c.Props.Find(p);
						if (Theirs)
						{
							if (!Mine) Props.Add(p, Mine = new StringsDef);
							*Mine = *Theirs;
						}
						else StillInherit++;
					}
					break;
				}
				InheritClass(TypeColor, ColorDef, ColorInherit);
				InheritClass(TypeImage, ImageDef, ImageInherit);
				InheritClass(TypeBorder, BorderDef, LenInherit);
				default:
				{
					LgiAssert(!"Not impl.");
					break;
				}
			}
		}
	}
	else LgiAssert(!"Not impl.");

	return StillInherit > 0;
}

bool GCss::CopyStyle(const GCss &c)
{
	Empty();

	int Prop;
	GCss &cc = (GCss&)c;
	for (void *p=cc.Props.First(&Prop); p; p=cc.Props.Next(&Prop))
	{
		switch (Prop >> 8)
		{
			#define CopyProp(TypeId, Type) \
				case TypeId: \
				{ \
					Type *n = new Type; \
					*n = *(Type*)p; \
					Props.Add(Prop, n); \
					break; \
				}

			case TypeEnum:
			{
				void *n = new DisplayType;
				*(uint32*)n = *(uint32*)p;
				Props.Add(Prop, n);
				break;
			}
			CopyProp(TypeLen, Len);
			CopyProp(TypeGRect, GRect);
			CopyProp(TypeColor, ColorDef);
			CopyProp(TypeImage, ImageDef);
			CopyProp(TypeBorder, BorderDef);
			CopyProp(TypeStrings, StringsDef);
			default:
			{
				LgiAssert(!"Invalidate property type.");
				return false;
			}
		}
	}

	return true;
}

bool GCss::operator ==(GCss &c)
{
	if (Props.Length() != c.Props.Length())
		return false;

	// Check individual types
	bool Eq = true;
	PropType Prop;
	for (void *Local=Props.First((int*)&Prop); Local && Eq; Local=Props.Next((int*)&Prop))
	{
		void *Other = c.Props.Find(Prop);
		if (!Other)
			return false;

		switch (Prop >> 8)
		{
			#define CmpType(id, type) \
				case id: \
				{ \
					if ( *((type*)Local) != *((type*)Other)) \
						Eq = false; \
					break; \
				}

			CmpType(TypeEnum, uint32);
			CmpType(TypeLen, Len);
			CmpType(TypeGRect, GRect);
			CmpType(TypeColor, ColorDef);
			CmpType(TypeImage, ImageDef);
			CmpType(TypeBorder, BorderDef);
			CmpType(TypeStrings, StringsDef);
			default:
				LgiAssert(!"Unknown type.");
				break;
		}
	}

	return Eq;
}

void GCss::Empty()
{
	int Prop;
	for (void *Data=Props.First(&Prop); Data; Data=Props.Next(&Prop))
	{
		int Type = Prop >> 8;
		switch (Type)
		{
			case TypeEnum:
				delete ((PropType*)Data);
				break;
			case TypeLen:
				delete ((Len*)Data);
				break;
			case TypeGRect:
				delete ((GRect*)Data);
				break;
			case TypeColor:
				delete ((ColorDef*)Data);
				break;
			case TypeImage:
				delete ((ImageDef*)Data);
				break;
			case TypeBorder:
				delete ((BorderDef*)Data);
				break;
			case TypeStrings:
				delete ((StringsDef*)Data);
				break;
			default:
				LgiAssert(!"Unknown property type.");
				break;
		}
	}
	Props.Empty();
}

void GCss::OnChange(PropType Prop)
{
}

bool GCss::Parse(char *&s, ParsingStyle Type)
{
	if (!s) return false;

	while (*s && *s != '}')
	{
		// Parse the prop name out
		SkipWhite(s);
		char Prop[32], *p = Prop;
		if (!*s) break;
		while (*s && (isalpha(*s) || strchr("-_", *s)))
			*p++ = *s++;
		*p++ = 0;
		SkipWhite(s);
		if (*s != ':')
			return false;
		s++;			
		PropType PropId = Lut.Find(Prop);
		PropTypes PropType = (PropTypes)((int)PropId >> 8);
		SkipWhite(s);
		char *ValueStart = s;

		// Do the data parsing based on type
		switch (PropType)
		{
			case TypeEnum:
			{
				switch (PropId)
				{
					case PropDisplay:
					{
						DisplayType *t = (DisplayType*)Props.Find(PropId);
						if (!t) Props.Add(PropId, t = new DisplayType);
						
						if (ParseWord(s, "block")) *t = DispBlock;
						else if (ParseWord(s, "inline")) *t = DispInline;
						else if (ParseWord(s, "inline-block")) *t = DispInlineBlock;
						else if (ParseWord(s, "list-item")) *t = DispListItem;
						else if (ParseWord(s, "none")) *t = DispNone;
						else *t = DispInherit;
						break;
					}
					case PropPosition:
					{
						PositionType *t = (PositionType*)Props.Find(PropId);
						if (!t) Props.Add(PropId, t = new PositionType);
						
						if (ParseWord(s, "static")) *t = PosStatic;
						else if (ParseWord(s, "relative")) *t = PosRelative;
						else if (ParseWord(s, "absolute")) *t = PosAbsolute;
						else if (ParseWord(s, "fixed")) *t = PosFixed;
						else *t = PosInherit;
						break;
					}
					case PropFloat:
					{
						FloatType *t = (FloatType*)Props.Find(PropId);
						if (!t) Props.Add(PropId, t = new FloatType);
						
						if (ParseWord(s, "left")) *t = FloatLeft;
						else if (ParseWord(s, "right")) *t = FloatRight;
						else if (ParseWord(s, "none")) *t = FloatNone;
						else *t = FloatInherit;
						break;
					}
					case PropFontStyle:
					{
						FontStyleType *w = (FontStyleType*)Props.Find(PropId);
						if (!w) Props.Add(PropId, w = new FontStyleType);

						     if (ParseWord(s, "inherit")) *w = FontStyleInherit;
						else if (ParseWord(s, "normal")) *w = FontStyleNormal;
						else if (ParseWord(s, "italic")) *w = FontStyleItalic;
						else if (ParseWord(s, "Oblique")) *w = FontStyleOblique;
						break;
					}
					case PropFontVariant:
					{
						FontVariantType *w = (FontVariantType*)Props.Find(PropId);
						if (!w) Props.Add(PropId, w = new FontVariantType);

						     if (ParseWord(s, "inherit")) *w = FontVariantInherit;
						else if (ParseWord(s, "normal")) *w = FontVariantNormal;
						else if (ParseWord(s, "small-caps")) *w = FontVariantSmallCaps;
						break;
					}
					case PropFontWeight:
					{
						FontWeightType *w = (FontWeightType*)Props.Find(PropId);
						if (!w) Props.Add(PropId, w = new FontWeightType);

						if (ParseWord(s, "Inherit")) *w = FontWeightInherit;
						else if (ParseWord(s, "Normal")) *w = FontWeightNormal;
						else if (ParseWord(s, "Bold")) *w = FontWeightBold;
						else if (ParseWord(s, "Bolder")) *w = FontWeightBolder;
						else if (ParseWord(s, "Lighter")) *w = FontWeightLighter;
						else if (ParseWord(s, "100")) *w = FontWeight100;
						else if (ParseWord(s, "200")) *w = FontWeight200;
						else if (ParseWord(s, "300")) *w = FontWeight300;
						else if (ParseWord(s, "400")) *w = FontWeight400;
						else if (ParseWord(s, "500")) *w = FontWeight500;
						else if (ParseWord(s, "600")) *w = FontWeight600;
						else if (ParseWord(s, "700")) *w = FontWeight700;
						else if (ParseWord(s, "800")) *w = FontWeight800;
						else if (ParseWord(s, "900")) *w = FontWeight900;
						break;
					}
					case PropTextDecoration:
					{
						TextDecorType *w = (TextDecorType*)Props.Find(PropId);
						if (!w) Props.Add(PropId, w = new TextDecorType);

						     if (ParseWord(s, "inherit")) *w = TextDecorInherit;
						else if (ParseWord(s, "none")) *w = TextDecorNone;
						else if (ParseWord(s, "underline")) *w = TextDecorUnderline;
						else if (ParseWord(s, "overline")) *w = TextDecorOverline;
						else if (ParseWord(s, "line-through")) *w = TextDecorLineThrough;
						else if (ParseWord(s, "blink")) *w = TextDecorBlink;
						break;
					}
					case PropWordWrap:
					{
						WordWrapType *w = (WordWrapType*)Props.Find(PropId);
						if (!w) Props.Add(PropId, w = new WordWrapType);

						if (ParseWord(s, "break-word")) *w = WrapBreakWord;
						else *w = WrapNormal;
						break;
					}
					case PropBorderCollapse:
					{
						BorderCollapseType *w = (BorderCollapseType*)Props.Find(PropId);
						if (!w) Props.Add(PropId, w = new BorderCollapseType);

						     if (ParseWord(s, "inherit")) *w = CollapseInherit;
						else if (ParseWord(s, "Collapse")) *w = CollapseCollapse;
						else if (ParseWord(s, "Separate")) *w = CollapseSeparate;
						break;
					}
					case PropBackgroundRepeat:
					{
						RepeatType *w = (RepeatType*)Props.Find(PropId);
						if (!w) Props.Add(PropId, w = new RepeatType);

						     if (ParseWord(s, "inherit")) *w = RepeatInherit;
						else if (ParseWord(s, "repeat-x")) *w = RepeatX;
						else if (ParseWord(s, "repeat-y")) *w = RepeatY;
						else if (ParseWord(s, "no-repeat")) *w = RepeatNone;
						else if (ParseWord(s, "repeat")) *w = RepeatBoth;
						break;
					}
					case PropListStyleType:
					case PropLetterSpacing:
					{
						// Fixme: do not care right now...
						break;
					}
					case PropBackgroundAttachment:
					{
						AttachmentType *w = (AttachmentType*)Props.Find(PropId);
						if (!w) Props.Add(PropId, w = new AttachmentType);

						     if (ParseWord(s, "inherit")) *w = AttachmentInherit;
						else if (ParseWord(s, "scroll")) *w = AttachmentScroll;
						else if (ParseWord(s, "fixed")) *w = AttachmentFixed;
						break;						
					}
					case PropOverflow:
					{
						OverflowType *w = (OverflowType*)Props.Find(PropId);
						if (!w) Props.Add(PropId, w = new OverflowType);

						     if (ParseWord(s, "inherit")) *w = OverflowInherit;
						else if (ParseWord(s, "visible")) *w = OverflowVisible;
						else if (ParseWord(s, "hidden")) *w = OverflowHidden;
						else if (ParseWord(s, "scroll")) *w = OverflowScroll;
						else if (ParseWord(s, "auto")) *w = OverflowAuto;
						break;
					}
					case PropVisibility:
					{
						VisibilityType *w = (VisibilityType*)Props.Find(PropId);
						if (!w) Props.Add(PropId, w = new VisibilityType);

						     if (ParseWord(s, "inherit")) *w = VisibilityInherit;
						else if (ParseWord(s, "visible")) *w = VisibilityVisible;
						else if (ParseWord(s, "hidden")) *w = VisibilityHidden;
						else if (ParseWord(s, "collapse")) *w = VisibilityCollapse;
						break;
					}
					case PropFont:
					{
						// FIXME
						break;
					}
					default:
					{
						LgiAssert(!"Prop parsing support not implemented.");
					}						
				}
				break;
			}
			case TypeLen:
			{
				GAutoPtr<Len> t(new Len);
				if (t->Parse(s, PropId == PropZIndex ? ParseRelaxed : Type))
				{
					if (PropId == PropPadding)
					{
						CopyPropOnSave(Len, PropPaddingLeft);
						CopyPropOnSave(Len, PropPaddingRight);
						CopyPropOnSave(Len, PropPaddingTop);
						ReleasePropOnSave(Len, PropPaddingBottom);
					}
					else if (PropId == PropMargin)
					{
						CopyPropOnSave(Len, PropMarginLeft);
						CopyPropOnSave(Len, PropMarginRight);
						CopyPropOnSave(Len, PropMarginTop);
						ReleasePropOnSave(Len, PropMarginBottom);
					}
					else
					{
						ReleasePropOnSave(Len, PropId);
					}
				}
				else if (Type == ParseStrict)
					LgiAssert(!"Parsing failed.");
				break;
			}
			case TypeColor:
			{
				GAutoPtr<ColorDef> t(new ColorDef);
				if (t->Parse(s) ||
					OnUnhandledColor(t, s))
				{
					ColorDef *e = (ColorDef*)Props.Find(PropId);
					if (e) *e = *t;
					else Props.Add(PropId, t.Release());
				}
				else if (Type == ParseStrict)
					LgiAssert(!"Parsing failed.");
				break;
			}
			case TypeStrings:
			{
				GAutoPtr<StringsDef> t(new StringsDef);
				if (t->Parse(s))
				{
					StringsDef *e = (StringsDef*)Props.Find(PropId);
					if (e) *e = *t;
					else Props.Add(PropId, t.Release());
				}
				else LgiAssert(!"Parsing failed.");
				break;
			}
			case TypeBorder:
			{
				GAutoPtr<BorderDef> t(new BorderDef);
				if (t->Parse(s))
				{
					if (PropId == PropBorder)
					{
						CopyPropOnSave(BorderDef, PropBorderLeft);
						CopyPropOnSave(BorderDef, PropBorderRight);
						CopyPropOnSave(BorderDef, PropBorderTop);
						ReleasePropOnSave(BorderDef, PropBorderBottom);
					}
					else
					{
						ReleasePropOnSave(BorderDef, PropId);
					}
				}
				break;
			}
			default:
			{
				if (Type == ParseStrict)
					LgiAssert(!"Unsupported property type.");
				break;
			}
		}

		// End of property delimiter
		while (*s && *s != ';') s++;
		if (*s != ';')
			break;
		s++;
	}

	return true;
}

/////////////////////////////////////////////////////////////////////////////
bool GCss::Len::Parse(char *&s, ParsingStyle ParseType)
{
	if (!s) return false;
	
	SkipWhite(s);
	if (ParseWord(s, "inherit")) Type = LenInherit;
	else if (ParseWord(s, "auto")) Type = LenInherit;
	else if (ParseWord(s, "normal")) Type = LenNormal;

	else if (ParseWord(s, "center")) Type = AlignCenter;
	else if (ParseWord(s, "left")) Type = AlignLeft;
	else if (ParseWord(s, "right")) Type = AlignRight;
	else if (ParseWord(s, "justify")) Type = AlignJustify;

	else if (ParseWord(s, "xx-small")) Type = SizeXXSmall;
	else if (ParseWord(s, "x-small")) Type = SizeXSmall;
	else if (ParseWord(s, "small")) Type = SizeSmall;
	else if (ParseWord(s, "medium")) Type = SizeMedium;
	else if (ParseWord(s, "large")) Type = SizeLarge;
	else if (ParseWord(s, "x-large")) Type = SizeXLarge;
	else if (ParseWord(s, "xx-large")) Type = SizeXXLarge;
	else if (ParseWord(s, "smaller")) Type = SizeSmaller;
	else if (ParseWord(s, "larger")) Type = SizeLarger;
	else if (IsNumeric(s))
	{
		Value = atof(s);
		while (IsNumeric(s)) s++;
		SkipWhite(s);
		if (*s == '%') Type = LenPercent;
		else if (ParseWord(s, "px")) Type = LenPx;
		else if (ParseWord(s, "pt")) Type = LenPt;
		else if (ParseWord(s, "em")) Type = LenEm;
		else if (ParseWord(s, "ex")) Type = LenEx;
		else if (ParseWord(s, "cm")) Type = LenCm;
		else if (ParseType == ParseRelaxed) Type = LenPx;
		else return false;
	}
	else return false;
		
	return true;
}

bool GCss::ColorDef::Parse(char *&s)
{
	if (!s) return false;

	#define NamedColour(Name, Value) \
		else if (ParseWord(s, #Name)) { Type = ColorRgb; Rgb32 = Value; return true; }
	#define ParseExpect(s, ch) \
		if (*s != ch) return false; \
		else s++;

	SkipWhite(s);
	if (*s == '#')
	{
		s++;
		int v = 0;
		char *e = s;
		while (*e && e < s + 6)
		{
			if (*e >= 'a' && *e <= 'f')
			{
				v <<= 4;
				v |= *e - 'a' + 10;
				e++;
			}
			else if (*e >= 'A' && *e <= 'F')
			{
				v <<= 4;
				v |= *e - 'A' + 10;
				e++;
			}
			else if (*e >= '0' && *e <= '9')
			{
				v <<= 4;
				v |= *e - '0';
				e++;
			}
			else break;
		}

		if (e == s + 3)
		{
			Type = ColorRgb;
			int r = (v >> 8) & 0xf;
			int g = (v >> 4) & 0xf;
			int b = v & 0xf;
			Rgb32 = Rgb32( (r << 4 | r), (g << 4 | g), (b << 4 | b) );
			s = e;
		}
		else if (e == s + 6)
		{
			Type = ColorRgb;
			int r = (v >> 16) & 0xff;
			int g = (v >> 8) & 0xff;
			int b = v & 0xff;
			Rgb32 = Rgb32(r, g, b);
			s = e;
		}
		else return false;
	}
	else if (ParseWord(s, "rgb") && *s == '(')
	{
		s++;
		int r = ParseComponent(s);
		ParseExpect(s, ',');
		int g = ParseComponent(s);
		ParseExpect(s, ',');
		int b = ParseComponent(s);
		ParseExpect(s, ')');
		
		Type = ColorRgb;
		Rgb32 = Rgb32(r, g, b);
	}
	else if (ParseWord(s, "rgba") && *s == '(')
	{
		s++;
		int r = ParseComponent(s);
		ParseExpect(s, ',');
		int g = ParseComponent(s);
		ParseExpect(s, ',');
		int b = ParseComponent(s);
		ParseExpect(s, ',');
		int a = ParseComponent(s);
		ParseExpect(s, ')');
		
		Type = ColorRgb;
		Rgb32 = Rgba32(r, g, b, a);
	}
	else if (ParseWord(s, "-webkit-gradient("))
	{
		GAutoString GradientType(ParseString(s));
		ParseExpect(s, ',');
		if (!GradientType)
			return false;
		if (!stricmp(GradientType, "radial"))
		{
		}
		else if (!stricmp(GradientType, "linear"))
		{
			Len StartX, StartY, EndX, EndY;
			if (!StartX.Parse(s) || !StartY.Parse(s))
				return false;
			ParseExpect(s, ',');
			if (!EndX.Parse(s) || !EndY.Parse(s))
				return false;
			ParseExpect(s, ',');
			SkipWhite(s);
			while (*s)
			{
				if (*s == ')')
				{
					Type = ColorLinearGradient;
					break;
				}
				else
				{
					GAutoString Stop(ParseString(s));
					if (!Stop) return false;
					if (!stricmp(Stop, "from"))
					{
					}
					else if (!stricmp(Stop, "to"))
					{
					}
					else if (!stricmp(Stop, "stop"))
					{
					}
					else return false;
				}
			}
		}
		else return false;
	}
	NamedColour(black, Rgb32(0x00, 0x00, 0x00))
	NamedColour(white, Rgb32(0xff, 0xff, 0xff))
	NamedColour(gray, Rgb32(0x80, 0x80, 0x80))
	NamedColour(red, Rgb32(0xff, 0x00, 0x00))
	NamedColour(yellow, Rgb32(0xff, 0xff, 0x00))
	NamedColour(green, Rgb32(0x00, 0x80, 0x00))
	NamedColour(orange, Rgb32(0xff, 0xA5, 0x00))
	NamedColour(blue, Rgb32(0x00, 0x00, 0xff))
	NamedColour(maroon, Rgb32(0x80, 0x00, 0x00))
	NamedColour(olive, Rgb32(0x80, 0x80, 0x00))
	NamedColour(purple, Rgb32(0x80, 0x00, 0x80))
	NamedColour(fuchsia, Rgb32(0xff, 0x00, 0xff))
	NamedColour(lime, Rgb32(0x00, 0xff, 0x00))
	NamedColour(navy, Rgb32(0x00, 0x00, 0x80))
	NamedColour(aqua, Rgb32(0x00, 0xff, 0xff))
	NamedColour(teal, Rgb32(0x00, 0x80, 0x80))
	NamedColour(silver, Rgb32(0xc0, 0xc0, 0xc0))
	else return false;

	return true;
}

bool GCss::ImageDef::Parse(char *&s)
{
	char Path[MAX_PATH];
	LgiGetSystemPath(LSP_APP_INSTALL, Path, sizeof(Path));

	SkipWhite(s);
	if (!strnicmp(s, "url(", 4))
	{
		s += 4;
		char *e = strchr(s, ')');
		if (!e)
			return false;
		GAutoString v(NewStr(s, e - s));
		LgiMakePath(Path, sizeof(Path), Path, v);
	}
	else LgiMakePath(Path, sizeof(Path), Path, s);

	if (Img = LoadDC(Path))
	{
		Type = ImageOwn;
	}

	return Img != 0;
}

bool GCss::BorderDef::Parse(char *&s)
{
	if (!s)
		return false;

	if (!Len::Parse(s, ParseRelaxed))
		return false;

	SkipWhite(s);
	if (!*s || *s == ';')
		return true;

	GAutoString Pat(ParseString(s));
	if (!Pat)
		return false;

	if (!stricmp(Pat, "Hidden")) Style = BorderHidden;
	else if (!stricmp(Pat, "Solid")) Style = BorderSolid;
	else if (!stricmp(Pat, "Dotted")) Style = BorderDotted;
	else if (!stricmp(Pat, "Dashed")) Style = BorderDashed;
	else if (!stricmp(Pat, "Double")) Style = BorderDouble;
	else if (!stricmp(Pat, "Groove")) Style = BorderGroove;
	else if (!stricmp(Pat, "Ridge")) Style = BorderRidge;
	else if (!stricmp(Pat, "Inset")) Style = BorderInset;
	else if (!stricmp(Pat, "Outset")) Style = BorderOutset;
	else Style = BorderSolid;

	if (!Color.Parse(s))
		return false;

	return true;
}
