#pragma once

#include "CoreMinimal.h"

/**
 * Holds the parsed bracket data: [ ... ] *N
 */
template<typename ElementType>
struct FBracketGroup
{
    TArray<ElementType> Values;
    int32 Repeats = 1; // default to 1
};

/**
 * Typedef for convenience to hold multiple bracket groups
 */
template<typename ElementType>
using FBracketedGroups = TArray<FBracketGroup<ElementType>>;

/**
 * Parses `InString` into bracket groups.
 * Each bracket group can have a suffix `*N` to indicate repeats.
 *
 * For example:
 *  - "[2 3] *3" => one bracket group with {2,3}, Repeats=3
 *  - "0 *2"     => single token group {0} with Repeats=2
 */
template<typename ElementType>
void ParseBracketedGroups(
    const FString& InString,
    /*OUT*/ FBracketedGroups<ElementType>& OutGroups
)
{
    OutGroups.Reset();

    // Tokenize the input (ignoring delimiter pin for demonstration).
    TArray<FString> Tokens;
    InString.ParseIntoArrayWS(Tokens);

    int32 i = 0;
    while (i < Tokens.Num())
    {
        const FString& Token = Tokens[i];
        // -------------------------------------------------------
        // Bracketed group: e.g. [2 3] *3
        // -------------------------------------------------------
        if (Token.StartsWith("["))
        {
            FBracketGroup<ElementType> Bracket;
            FString Trimmed = Token;
            Trimmed.RemoveFromStart("[");
            bool bEndFound = false;
            bool bCheckForRepeat = false;

            if (Trimmed.EndsWith("]"))
            {
                Trimmed.RemoveFromEnd("]");
                bEndFound = true;
                bCheckForRepeat = true;
            }

            // Parse the first chunk if not empty
            if (!Trimmed.IsEmpty())
            {
                ElementType Parsed{};
                LexTryParseString(Parsed, *Trimmed);
                Bracket.Values.Add(Parsed);
            }
            ++i;

            // Read until we see a ']'
            while (!bEndFound && i < Tokens.Num())
            {
                FString NextTok = Tokens[i];
                if (NextTok.EndsWith("]"))
                {
                    NextTok.RemoveFromEnd("]");
                    bEndFound = true;
                    bCheckForRepeat = true;
                }

                if (!NextTok.IsEmpty())
                {
                    ElementType SubParsed{};
                    LexTryParseString(SubParsed, *NextTok);
                    Bracket.Values.Add(SubParsed);
                }
                ++i;
            }

            // Check for the optional *N repeat right after the bracket
            if (bCheckForRepeat && i < Tokens.Num())
            {
                const FString& PotentialRepeat = Tokens[i];
                if (PotentialRepeat.StartsWith("*") && PotentialRepeat.Len() > 1)
                {
                    FString NumberPart = PotentialRepeat.Mid(1); // remove '*'
                    int32 LocalRepeat = 1;
                    if (LexTryParseString(LocalRepeat, *NumberPart))
                    {
                        Bracket.Repeats = FMath::Max(LocalRepeat, 1);
                        ++i;
                    }
                }
            }

            OutGroups.Add(MoveTemp(Bracket));
        }
        // -------------------------------------------------------
        // Single-token group: e.g. "0 *2"
        // -------------------------------------------------------
        else
        {
            FBracketGroup<ElementType> SingleGrp;
            ElementType Parsed{};
            LexTryParseString(Parsed, *Token);
            SingleGrp.Values.Add(Parsed);
            ++i;

            // Check if next token is *N
            if (i < Tokens.Num())
            {
                const FString& PotentialRepeat = Tokens[i];
                if (PotentialRepeat.StartsWith("*") && PotentialRepeat.Len() > 1)
                {
                    FString NumberPart = PotentialRepeat.Mid(1); 
                    int32 LocalRepeat = 1;
                    if (LexTryParseString(LocalRepeat, *NumberPart))
                    {
                        SingleGrp.Repeats = FMath::Max(LocalRepeat, 1);
                        ++i;
                    }
                }
            }

            OutGroups.Add(MoveTemp(SingleGrp));
        }
    }
}