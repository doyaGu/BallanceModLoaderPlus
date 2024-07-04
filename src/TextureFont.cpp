#include "TextureFont.h"
#include "FontManager.h"
#include "CKRasterizer.h"

TextureFont::TextureFont(FontManager *fm, CKContext *ctx, char *name) : m_FontCoordinates() {
    // Font Visual Properties
    m_Leading.Set(0.0f, 0.0f);
    m_Scale.Set(1.0f, 1.0f);
    m_ShadowOffset.Set(4.0f, 4.0f);
    m_ShadowScale.Set(1.0f, 1.0f);
    m_ItalicOffset = 0.0f;
    m_StartColor = RGBAITOCOLOR(255, 255, 255, 255);
    m_EndColor = RGBAITOCOLOR(0, 0, 0, 255);
    m_ShadowColor = RGBAITOCOLOR(0, 0, 0, 128);
    m_Material = 0;
    m_Properties = 0;
    m_FontTexture = 0;
    m_FirstCharacter = 0;
    m_ParagraphIndentation = Vx2DVector(0.0f);
    m_SpacingProperties = 0;
    m_CaretMaterial = nullptr;
    m_CaretSize = 0.0f;
    m_SpacePercentage = 0.3f;
    m_LineCount = 0;
    m_SpaceSize = 0.0f;
    m_HLeading = 0.0f;
    m_LineWidth = 0.0f;

    m_FontName = CKStrdup(name);
    m_SystemFontName = nullptr;
    m_Context = ctx;
    m_FontManager = fm;
}

TextureFont::~TextureFont() {
    CKDeletePointer(m_FontName);
}

// Font Access
char *TextureFont::GetFontName() const {
    return m_FontName;
}

CKBOOL TextureFont::IsFontSimilar(CKTexture *fontTexture, Vx2DVector &charNumber, CKBOOL fixed) const {
    if (fixed) {
        if (!(m_SpacingProperties & FIXED))
            return FALSE;
    } else {
        if (m_SpacingProperties & FIXED)
            return FALSE;
    }
    if (m_FontTexture != CKOBJID(fontTexture))
        return FALSE;

    return TRUE;
}

void TextureFont::CreateCKFont(CKTexture *fontTexture, VxRect &textZone, Vx2DVector &charNumber, CKBOOL fixed,
                               int firstCharacter, float iSpaceSize) {
    if (!fontTexture)
        return;

    m_CharNumber = charNumber;
    m_SpacingProperties = (fixed) ? FIXED : 0;
    m_FirstCharacter = firstCharacter;

    // save the font texture id
    m_FontTexture = CKOBJID(fontTexture);

    float twidth = (float) fontTexture->GetWidth();
    float theight = (float) fontTexture->GetHeight();

    m_ScreenExtents.Set(twidth, theight);

    if ((int) textZone.GetWidth() == 0 || (int) textZone.GetHeight() == 0) {
        textZone.SetDimension(0, 0, twidth, theight);
    }

    m_FontZone = textZone;
    m_SpacePercentage = iSpaceSize;

    // the Creation
    CreateFromTexture();
}

void TextureFont::CreateFromTexture() {
    if (m_SpacingProperties & CREATED)
        return;

    CKTexture *fontTexture = (CKTexture *) m_Context->GetObject(m_FontTexture);
    if (!fontTexture)
        return;

    int iWidth = fontTexture->GetWidth();

    float twidth = (float) iWidth;
    float theight = (float) fontTexture->GetHeight();

    m_ScreenExtents.Set(twidth, theight);

    float ustep = m_FontZone.GetWidth() / (twidth * m_CharNumber.x);
    float vstep = m_FontZone.GetHeight() / (theight * m_CharNumber.y);

    Vx2DVector v2 = m_FontZone.GetTopLeft();
    float u = v2.x / twidth;
    float v = v2.y / theight;

    int c = m_FirstCharacter;

    // Initialisation of the characters
    for (int k = 0; k < 256; ++k) {
        m_FontCoordinates[k].ustart = u;
        m_FontCoordinates[k].vstart = v;
        m_FontCoordinates[k].uwidth = ustep;
        m_FontCoordinates[k].uprewidth = 0;
        m_FontCoordinates[k].upostwidth = 0;
        m_FontCoordinates[k].vwidth = vstep;
    }

    if (m_SpacingProperties & FIXED) { // The font must be fixed
        // fill the uvs with the characters
        for (int i = 0; (float) i < m_CharNumber.y; ++i) {
            for (int j = 0; (float) j < m_CharNumber.x; ++j) {
                m_FontCoordinates[c].ustart = u;
                m_FontCoordinates[c].vstart = v;

                m_FontCoordinates[c].uwidth = ustep;
                m_FontCoordinates[c].vwidth = vstep;

                u += ustep;
                c++;
            }
            u = 0.0f;
            v += vstep;
        }
    } else { // The font must be proportional
        CKDWORD transColor = 0;
        CKBOOL alpha = TRUE;
        if (fontTexture->IsTransparent()) {
            alpha = FALSE;
            transColor = fontTexture->GetTransparentColor();
        }

        float width = (float) fontTexture->GetWidth();
        float height = (float) fontTexture->GetHeight();

        float upixel = 1.0f / width;
        float vpixel = 1.0f / height;

        int xpixel = 0;
        int ypixel = 0;

        int xwidth = (int) (width * ustep);
        int ywidth = (int) (height * vstep);

        {
            CKDWORD *pixelMap = (CKDWORD *) fontTexture->LockSurfacePtr();

            // fill the uvs with the characters
            for (int i = 0; i < m_CharNumber.y; ++i) {
                for (int j = 0; j < m_CharNumber.x; ++j) {
                    m_FontCoordinates[c].ustart = u;
                    m_FontCoordinates[c].vstart = v;

                    m_FontCoordinates[c].uwidth = ustep;
                    m_FontCoordinates[c].vwidth = vstep;

                    // We now try to narrow the character

                    // left
                    int k;
                    for (k = 0; k < xwidth; k++) {
                        CKDWORD color;
                        int y;
                        if (alpha) {
                            for (y = 0; y < ywidth; y++) {
                                // color = fontTexture->GetPixel(xpixel+k,ypixel+y);
                                color = pixelMap[xpixel + k + iWidth * (ypixel + y)];
                                if (ColorGetAlpha(color))
                                    break;
                            }
                        } else {
                            for (y = 0; y < ywidth; y++) {
                                // color = fontTexture->GetPixel(xpixel+k,ypixel+y);
                                color = pixelMap[xpixel + k + iWidth * (ypixel + y)];
                                if (color != transColor)
                                    break;
                            }
                        }
                        if (y < ywidth)
                            break;
                    }

                    if (k == xwidth) { // the whole character is empty
                        m_FontCoordinates[c].uwidth *= m_SpacePercentage; // Changed from 0.5 to 0.3 coz was too big
                        u += ustep;
                        xpixel += xwidth;

                        c++;
                        // We go on to the next character
                        continue;
                    } else {
                        m_FontCoordinates[c].ustart += k * upixel;
                        m_FontCoordinates[c].uwidth -= k * upixel;
                    }

                    // right
                    for (k = 0; k < xwidth; k++) {
                        int y;
                        for (y = 0; y < ywidth; y++) {
                            // CKDWORD color = fontTexture->GetPixel(xpixel+xwidth-1-k,ypixel+y);
                            CKDWORD color = pixelMap[xpixel + xwidth - 1 - k + iWidth * (ypixel + y)];
                            if (alpha) {
                                if (ColorGetAlpha(color))
                                    break;
                            } else {
                                if (color != transColor)
                                    break;
                            }
                        }
                        if (y < ywidth)
                            break;
                    }

                    m_FontCoordinates[c].uwidth -= k * upixel;
                    if (m_FontCoordinates[c].uwidth < 0.0f)
                        m_FontCoordinates[c].uwidth = 0.0f;

                    u += ustep;
                    xpixel += xwidth;

                    c++;
                }

                u = 0.0f;
                xpixel = 0;

                v += vstep;
                ypixel += ywidth;
            }

            fontTexture->ReleaseSurfacePtr();
        }
    }

    if (m_FirstCharacter) {
        m_FontCoordinates[0].vwidth = vstep;
    }

    // The font is now officially created
    m_SpacingProperties |= CREATED;
}