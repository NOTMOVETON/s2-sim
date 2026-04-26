#include <gtest/gtest.h>
#include <s2/zone.hpp>

TEST(ZoneSelfDestruct, DefaultIsNone)
{
    s2::Zone z;
    EXPECT_EQ(z.self_destruct.type, s2::SelfDestructPolicy::Type::NONE);
}

TEST(ZoneSelfDestruct, CanSetOnAnyContact)
{
    s2::Zone z;
    z.self_destruct.type = s2::SelfDestructPolicy::Type::ON_ANY_CONTACT;
    EXPECT_EQ(z.self_destruct.type, s2::SelfDestructPolicy::Type::ON_ANY_CONTACT);
}

TEST(ZoneSelfDestruct, CanSetOnEffectApplied)
{
    s2::Zone z;
    z.self_destruct.type = s2::SelfDestructPolicy::Type::ON_EFFECT_APPLIED;
    EXPECT_EQ(z.self_destruct.type, s2::SelfDestructPolicy::Type::ON_EFFECT_APPLIED);
}
