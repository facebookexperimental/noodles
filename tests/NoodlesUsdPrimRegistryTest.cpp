// Copyright (c) Meta Platforms, Inc. and affiliates.
//
// This source code is licensed under the MIT license found in the
// LICENSE file in the root directory of this source tree.

#include <gtest/gtest.h>

#include "usd/UsdPrimRegistry.h"

#include <pxr/usd/sdf/valueTypeName.h>
#include <pxr/usd/usd/attribute.h>
#include <pxr/usd/usd/stage.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <unordered_set>

PXR_NAMESPACE_USING_DIRECTIVE

class UsdPrimRegistryStaticTest : public ::testing::Test {
 protected:
  UsdStageRefPtr stage;

  void SetUp() override {
    stage = UsdStage::CreateInMemory();
  }

  void TearDown() override {
    stage.Reset();
  }
};

TEST_F(UsdPrimRegistryStaticTest, SchemaAwarePropertyNames_LightHasInputPins) {
  auto prim = stage->DefinePrim(SdfPath("/Light"), TfToken("GeometryLight"));
  auto names = UsdPrimRegistry::GetSchemaAwarePropertyNames(prim);

  std::unordered_set<std::string> nameSet;
  for (const auto& tok : names) {
    nameSet.insert(tok.GetString());
  }
  EXPECT_TRUE(nameSet.count("inputs:color"))
      << "GeometryLight should have inputs:color from LightAPI";
  EXPECT_TRUE(nameSet.count("inputs:intensity"))
      << "GeometryLight should have inputs:intensity from LightAPI";
}

TEST_F(UsdPrimRegistryStaticTest, SchemaAwarePropertyNames_NoDuplicates) {
  auto prim = stage->DefinePrim(SdfPath("/Light"), TfToken("GeometryLight"));
  prim.CreateAttribute(TfToken("inputs:color"), SdfValueTypeNames->Color3f);

  auto names = UsdPrimRegistry::GetSchemaAwarePropertyNames(prim);

  int colorCount = 0;
  for (const auto& tok : names) {
    if (tok.GetString() == "inputs:color") {
      ++colorCount;
    }
  }
  EXPECT_EQ(colorCount, 1) << "inputs:color should appear exactly once";
}

TEST_F(UsdPrimRegistryStaticTest, SchemaAwarePropertyNames_ExcludesRelationships) {
  auto prim = stage->DefinePrim(SdfPath("/M"), TfToken("Mesh"));
  auto names = UsdPrimRegistry::GetSchemaAwarePropertyNames(prim);

  std::unordered_set<std::string> nameSet;
  for (const auto& tok : names) {
    nameSet.insert(tok.GetString());
  }
  EXPECT_FALSE(nameSet.count("proxyPrim")) << "proxyPrim is a relationship and should be excluded";
}

TEST_F(UsdPrimRegistryStaticTest, SchemaAwarePropertyNames_AuthoredAppended) {
  auto prim = stage->DefinePrim(SdfPath("/N"));
  prim.CreateAttribute(TfToken("inputs:custom"), SdfValueTypeNames->Float);

  auto names = UsdPrimRegistry::GetSchemaAwarePropertyNames(prim);

  std::unordered_set<std::string> nameSet;
  for (const auto& tok : names) {
    nameSet.insert(tok.GetString());
  }
  EXPECT_TRUE(nameSet.count("inputs:custom")) << "Authored attribute should appear in the result";
}

TEST_F(UsdPrimRegistryStaticTest, SchemaAwarePropertyNames_SchemaBeforeAuthored) {
  auto prim = stage->DefinePrim(SdfPath("/L"), TfToken("GeometryLight"));
  prim.CreateAttribute(TfToken("inputs:custom"), SdfValueTypeNames->Float);

  auto names = UsdPrimRegistry::GetSchemaAwarePropertyNames(prim);

  int schemaPos = -1;
  int authoredPos = -1;
  for (int i = 0; i < static_cast<int>(names.size()); ++i) {
    if (names[i] == "inputs:intensity") {
      schemaPos = i;
    }
    if (names[i] == "inputs:custom") {
      authoredPos = i;
    }
  }
  ASSERT_NE(schemaPos, -1) << "Schema attribute inputs:intensity should be present";
  ASSERT_NE(authoredPos, -1) << "Authored attribute inputs:custom should be present";
  EXPECT_LT(schemaPos, authoredPos)
      << "Schema properties must appear before authored-only properties";
}

TEST_F(UsdPrimRegistryStaticTest, GetAttributeTypeNameString_SchemaOnlyAttr) {
  auto prim = stage->DefinePrim(SdfPath("/Light"), TfToken("GeometryLight"));
  auto typeName = UsdPrimRegistry::GetAttributeTypeNameString(prim, TfToken("inputs:color"));
  EXPECT_EQ(typeName, "color3f");
}

TEST_F(UsdPrimRegistryStaticTest, GetAttributeTypeNameString_AuthoredAttr) {
  auto prim = stage->DefinePrim(SdfPath("/N"));
  prim.CreateAttribute(TfToken("inputs:val"), SdfValueTypeNames->Float);

  auto typeName = UsdPrimRegistry::GetAttributeTypeNameString(prim, TfToken("inputs:val"));
  EXPECT_EQ(typeName, "float");
}

TEST_F(UsdPrimRegistryStaticTest, GetAttributeTypeNameString_UnknownReturnsEmpty) {
  auto prim = stage->DefinePrim(SdfPath("/Light"), TfToken("GeometryLight"));
  auto typeName = UsdPrimRegistry::GetAttributeTypeNameString(prim, TfToken("inputs:nonexistent"));
  EXPECT_TRUE(typeName.empty());
}

TEST_F(UsdPrimRegistryStaticTest, GetDescriptorFromPrim_LightDeduplicatesPins) {
  auto prim = stage->DefinePrim(SdfPath("/Light"), TfToken("GeometryLight"));
  prim.CreateAttribute(TfToken("inputs:color"), SdfValueTypeNames->Color3f);

  UsdPrimRegistry registry;
  auto desc = registry.GetDescriptorFromPrim(prim);

  int colorCount = 0;
  for (const auto& pin : desc.inputPins) {
    if (pin == "color") {
      ++colorCount;
    }
  }
  EXPECT_EQ(colorCount, 1) << "color pin should appear exactly once in descriptor";
  EXPECT_EQ(desc.inputPinTypes["color"], "color3f") << "descriptor should map color pin to color3f";
}
