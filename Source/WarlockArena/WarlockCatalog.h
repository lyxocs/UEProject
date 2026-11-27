#pragma once
#include "CoreMinimal.h"
class AWarlockState;
namespace WarlockCatalog
{
enum EFamily {Fireball,Smite,Lightning,Blink,Spray,Meteor,Reflect,Chain,Haste,Boomerang,Seeker,Dash,Swap,Drain,Bounce,WindWalk,Split,Blast,Rewind,Mirror,Bind,Gravity,Control,Count};
int Family(const AWarlockState* S,int Slot);
int Product(int Family);
int FromProduct(int Product);
int Slot(int Family);
const TCHAR* Name(int Family);
float Cooldown(int Family,int Level,bool Alternate);
int Cap(int Family);
int Cost(int Family,int Level,int LearnedCount);
bool Dual(int Family);
FString Gate(const AWarlockState* S,int Family);
TArray<int> Families(int Slot);
FString ModeName(int Family,bool Alternate);
FString Description(int Family,int Level,bool Alternate);
}
