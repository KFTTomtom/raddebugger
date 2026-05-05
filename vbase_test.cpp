#include <cstdio>
#include <cstdint>
#include <cstddef>
#include <vector>

struct FGameplayTagContainer {
    std::vector<uint32_t> Tags;
};

struct FImpulseParameters {
    float StrengthX = 0.f;
    float StrengthY = 0.f;
    float StrengthZ = 0.f;
};

struct FCollisionShape {
    float HalfExtentX = 0.f;
    float HalfExtentY = 0.f;
    float HalfExtentZ = 0.f;
};

struct IMassDamageEvent {
    virtual ~IMassDamageEvent() = default;
    virtual float GetDamage() const { return 0.f; }

    FImpulseParameters ImpulseParameters = {1.0f, 2.0f, 3.0f};
    FImpulseParameters DeathImpulseParameters = {4.0f, 5.0f, 6.0f};
    FCollisionShape DamageShape = {10.f, 20.f, 30.f};
    FGameplayTagContainer DamageTags;
    float StaggerDamage = 42.5f;
    int32_t AttackUniqueID = 9999;
};

struct FPointDamageEvent {
    float Damage = 100.0f;
    int32_t HitBoneIndex = 7;
};

struct FMassPointDamageEvent : public FPointDamageEvent, public virtual IMassDamageEvent {
    float GetDamage() const override { return Damage; }
    virtual float GetDistance() const { return 0.f; }
};

struct IKFTDamageEvent : public virtual IMassDamageEvent {
    virtual ~IKFTDamageEvent() = default;

    int32_t TrackIndex = 64;
    int32_t NPCArchetypeID = 291;
    int64_t NPCStatusEffects = 0xDEADBEEF;
    int64_t PlayerStatusEffects = 0xCAFEBABE;
    int64_t PlayerAttackID = 0x12345678;
};

struct FKFTPointDamageEvent : public FMassPointDamageEvent, public IKFTDamageEvent {
    static const int32_t ClassID = 5;
    float GetDamage() const override { return Damage * 1.5f; }
    int32_t ExtraField = 777;
};

__declspec(noinline) void inspect_ikft(const IKFTDamageEvent* evt) {
    printf("--- inspect via IKFTDamageEvent* ---\n");
    printf("TrackIndex       = %d   (expect 64)\n", evt->TrackIndex);
    printf("StaggerDamage    = %.1f  (expect 42.5)\n", evt->StaggerDamage);
    printf("ImpulseParams    = (%.1f, %.1f, %.1f) (expect 1,2,3)\n",
           evt->ImpulseParameters.StrengthX, evt->ImpulseParameters.StrengthY, evt->ImpulseParameters.StrengthZ);
}

__declspec(noinline) void inspect_concrete(const FKFTPointDamageEvent* evt) {
    printf("--- inspect via FKFTPointDamageEvent* ---\n");
    printf("Damage           = %.1f  (expect 150.0)\n", evt->Damage);
    printf("ExtraField       = %d   (expect 777)\n", evt->ExtraField);
    printf("TrackIndex       = %d   (expect 64)\n", evt->TrackIndex);
    printf("StaggerDamage    = %.1f  (expect 42.5)\n", evt->StaggerDamage);
    printf("ImpulseParams    = (%.1f, %.1f, %.1f) (expect 1,2,3)\n",
           evt->ImpulseParameters.StrengthX, evt->ImpulseParameters.StrengthY, evt->ImpulseParameters.StrengthZ);
    printf("DamageTags.size  = %zu  (expect 5)\n", evt->DamageTags.Tags.size());
}

int main() {
    FKFTPointDamageEvent dmg;
    dmg.Damage = 150.0f;
    dmg.HitBoneIndex = 12;
    dmg.TrackIndex = 64;
    dmg.NPCArchetypeID = 291;
    dmg.ExtraField = 777;
    dmg.DamageTags.Tags = {111, 222, 333, 444, 555};

    printf("sizeof(FKFTPointDamageEvent) = %zu\n", sizeof(FKFTPointDamageEvent));
    printf("sizeof(IKFTDamageEvent)      = %zu\n", sizeof(IKFTDamageEvent));
    printf("sizeof(IMassDamageEvent)     = %zu\n", sizeof(IMassDamageEvent));

    inspect_ikft(&dmg);
    inspect_concrete(&dmg);

    return 0;
}
