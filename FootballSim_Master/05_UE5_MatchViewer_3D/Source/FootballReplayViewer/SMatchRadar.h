#pragma once
#include "Widgets/SLeafWidget.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"
#include "FSimReplay.h"

// Pure Slate overlay: the stadium camera never renders token meshes.
class SMatchRadar : public SLeafWidget
{
public:
    SLATE_BEGIN_ARGS(SMatchRadar) {} SLATE_ARGUMENT(const FFSimReplayFrame*, Frame) SLATE_END_ARGS()
    void Construct(const FArguments& Args) { Frame = Args._Frame; }
    virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D(240,150); }
    virtual int32 OnPaint(const FPaintArgs&, const FGeometry& G, const FSlateRect&, FSlateWindowElementList& Out,
        int32 Layer, const FWidgetStyle&, bool) const override
    {
        const FVector2D Size = G.GetLocalSize();
        const auto Point = [&Size](double X,double Z) { return FVector2D(9+(X/105+0.5)*(Size.X-18),9+(0.5-Z/68)*(Size.Y-18)); };
        const auto Line = [&](TArray<FVector2D> Points,FLinearColor Color,float Width=1)
        { FSlateDrawElement::MakeLines(Out,Layer+1,G.ToPaintGeometry(),Points,ESlateDrawEffect::None,Color,true,Width); };
        const auto Box = [&](FVector2D P,FVector2D S,FLinearColor C,int32 L)
        { FSlateDrawElement::MakeBox(Out,L,G.ToPaintGeometry(S,FSlateLayoutTransform(P)),FCoreStyle::Get().GetBrush("WhiteBrush"),ESlateDrawEffect::None,C); };
        Box(FVector2D::ZeroVector,Size,FLinearColor(0.008f,0.025f,0.022f,0.66f),Layer);
        const FLinearColor White(0.75f,0.85f,0.8f,0.55f);
        Line({Point(-52.5,-34),Point(52.5,-34),Point(52.5,34),Point(-52.5,34),Point(-52.5,-34)},White);
        Line({Point(0,-34),Point(0,34)},White);
        TArray<FVector2D> Circle;
        for(int32 I=0;I<=40;++I) Circle.Add(Point(9.15*FMath::Cos(I*2*PI/40),9.15*FMath::Sin(I*2*PI/40)));
        Line(Circle,White);
        for(double Side:{-1.0,1.0}) Line({Point(Side*52.5,-20.16),Point(Side*36,-20.16),Point(Side*36,20.16),Point(Side*52.5,20.16)},White);
        if(Frame)
        {
            for(int32 I=0;I<22;++I)
            {
                const auto P=Point(Frame->Players[I].X,Frame->Players[I].Z);
                Box(P-FVector2D(3.5),FVector2D(7),FLinearColor(0,0,0,0.7f),Layer+2);
                Box(P-FVector2D(2.5),FVector2D(5),I<11?FLinearColor(0.12f,0.55f,1):FLinearColor(1,0.15f,0.08f),Layer+3);
            }
            Box(Point(Frame->Ball.X,Frame->Ball.Z)-FVector2D(2),FVector2D(4),FLinearColor::White,Layer+4);
        }
        return Layer+4;
    }
private:
    const FFSimReplayFrame* Frame = nullptr;
};
