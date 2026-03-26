#include "Text/TextEncoding.h"

#include "EngineCore/Logger.h"

FU32String Utf8ToCodepoints(const FString& Text)
{
    FU32String Output;

    const uint32 Length = static_cast<uint32>(Text.size());

    for (uint32 i = 0; i < Length;)
    {
        const uint8 b0 = static_cast<uint8>(Text[i]);
        uint32 Codepoint = 0;

        // UTF-8은 MSB 쪽의 비트 패턴을 통해 글자의 길이를 판별
        if (b0 < 0x80)
        {
            // 1-byte
            Codepoint = b0;

            i++;
        }
        else if ((b0 & 0xE0) == 0xC0)
        {
            // 2-byte
            if (i + 1 >= Length)
            {
                UE_LOG("[Text/TextEncoding][Error] Unexpected UTF-8 bit pattern!");

                Codepoint = REP_CHR;

                i += 1;
            }
            else
            {
                const uint8 b1 = static_cast<uint8>(Text[i + 1]);

                // UTF-8은 뒤에 연결되는 비트 패턴도 0x80을 검사해야 함
                if ((b1 & 0xC0) != 0x80)
                {
                    UE_LOG("[Text/TextEncoding][Error] Unexpected UTF-8 bit pattern!");

                    Codepoint = REP_CHR;

                    i += 1;
                }
                else
                {
                    // Payload는 때고 붙임
                    Codepoint = ((b0 & 0x1F) << 6) | (b1 & 0x3F);

                    // 2-byte는 최소 U+0080 이상이어야 함(Overlong encoding)
                    if (Codepoint < 0x80)
                    {
                        UE_LOG("[Text/TextEncoding][Error] Unexpected UTF-8 bit pattern!");

                        Codepoint = REP_CHR;
                        
                        i += 1;
                    }
                    else
                    {
                        i += 2;
                    }
                }
            }
        }
        else if ((b0 & 0xF0) == 0xE0)
        {
            // 3-byte
            if (i + 2 >= Length)
            {
                UE_LOG("[Text/TextEncoding][Error] Unexpected UTF-8 bit pattern!");
                
                Codepoint = REP_CHR;

                i += 1;
            }
            else
            {
                const uint8 b1 = static_cast<uint8>(Text[i + 1]);
                const uint8 b2 = static_cast<uint8>(Text[i + 2]);

                if ((b1 & 0xC0) != 0x80 || (b2 & 0xC0) != 0x80)
                {
                    UE_LOG("[Text/TextEncoding][Error] Unexpected UTF-8 bit pattern!");

                    Codepoint = REP_CHR;
                    
                    i += 1;
                }
                else
                {
                    Codepoint = ((b0 & 0x0F) << 12) | ((b1 & 0x3F) << 6) | (b2 & 0x3F);

                    // 3-byte는 최소 U+0800 이상이어야 함
                    // + UTF-16 surrogate range(문자로 쓸 수 없는 범위 / U+D800 ~ U+DFFF)는 invalid
                    if (Codepoint < 0x800 || (Codepoint >= 0xD800 && Codepoint <= 0xDFFF))
                    {
                        UE_LOG("[Text/TextEncoding][Error] Unexpected UTF-8 bit pattern!");

                        Codepoint = REP_CHR;
                        
                        i += 1;
                    }
                    else
                    {
                        i += 3;
                    }
                }
            }
        }
        else if ((b0 & 0xF8) == 0xF0)
        {
            // 4-byte
            if (i + 3 >= Length)
            {
                UE_LOG("[Text/TextEncoding][Error] Unexpected UTF-8 bit pattern!");

                Codepoint = REP_CHR;
                
                i += 1;
            }
            else
            {
                const uint8 b1 = static_cast<uint8>(Text[i + 1]);
                const uint8 b2 = static_cast<uint8>(Text[i + 2]);
                const uint8 b3 = static_cast<uint8>(Text[i + 3]);

                if ((b1 & 0xC0) != 0x80 || (b2 & 0xC0) != 0x80 || (b3 & 0xC0) != 0x80)
                {
                    UE_LOG("[Text/TextEncoding][Error] Unexpected UTF-8 bit pattern!");

                    Codepoint = REP_CHR;
                    
                    i += 1;
                }
                else
                {
                    Codepoint = ((b0 & 0x07) << 18) | ((b1 & 0x3F) << 12) | ((b2 & 0x3F) << 6) | (b3 & 0x3F);

                    // 4-byte는 최소 U+10000 이상
                    // + Unicode 최대 범위는 U+10FFFF
                    if (Codepoint < 0x10000 || Codepoint > 0x10FFFF)
                    {
                        UE_LOG("[Text/TextEncoding][Error] Unexpected UTF-8 bit pattern!");

                        Codepoint = REP_CHR;
                        
                        i += 1;
                    }
                    else
                    {
                        i += 4;
                    }
                }
            }
        }
        else
        {
            // invalid UTF-8
            UE_LOG("[Text/TextEncoding][Error] Unexpected UTF-8 bit pattern!");

            Codepoint = REP_CHR;
            
            i += 1;
        }

        Output.push_back(static_cast<char32_t>(Codepoint));
    }

    return Output;
}
