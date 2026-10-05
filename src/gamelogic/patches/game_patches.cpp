#include <cstdint>
#include <string>

#include <rex/cvar.h>
#include <rex/hook.h>

#include "core/logging.h"
#include "core/memory_helpers.h"
#include "gamelogic/ui/hud_api.h"

REXCVAR_DEFINE_BOOL(eot_sd_suits, false, "EdgeOfTime/Config", "Unlock Shattered Dimensions suits");

REXCVAR_DEFINE_STRING(eot_sd_code, "excelsior", "EdgeOfTime/Config", "Shattered Dimensions unlock code");

REX_EXTERN(__imp__eot_HUDStartScreen_EnterSDCheck); // (this r3)
REX_EXTERN(__imp__eot_SaveGame_ApplyLoadedData);    // (this r3, result r4)
REX_EXTERN(__imp__eot_HUDDLCCode_Validate);
REX_EXTERN(__imp__eot_TextPopup_InitConfig);        // (config r3, input block r4)
REX_EXTERN(__imp__eot_TextPopup_SetBody);           // (config r3, string handle r4)
REX_EXTERN(__imp__eot_YesNoWindow_Open);            // (config r3) -> pop-up id

namespace {

constexpr uint32_t kSdSaveSeen = 0x883DDC48;
constexpr uint32_t kBonusFlags = 0x883DD42C;
constexpr uint16_t kBonusSdWelcome = 0x100;

constexpr uint32_t kPageTyped = 220;
constexpr uint32_t kPageConfig = 1248;
constexpr uint32_t kPagePriority = 1252;
constexpr uint32_t kPageFlags = 1276;
constexpr uint32_t kPageTitleIndex = 1324;
constexpr uint32_t kPagePopupId = 1352;
constexpr uint32_t kPageAlreadyHandle = 1360;
constexpr uint32_t kPageResult = 1364;
constexpr uint32_t kConfigTitles = 32;
constexpr uint32_t kVipTitleNameCrc = 0xD67C58C4;
constexpr uint32_t kSuitUnlockedNameCrc = 0x97C2F465;

void ForceFlag(const char *where) {
  if (!REXCVAR_GET(eot_sd_suits) || eot::mem::load<uint8_t>(kSdSaveSeen) != 0)
    return;
  eot::mem::store<uint8_t>(kSdSaveSeen, 1);
  EOT_INFO("[sd] Shattered Dimensions bonus suits: flag set {}", where);
}

std::string TypedCode(uint32_t self) {
  std::string out;
  for (uint32_t i = 0; i < 128; ++i) {
    const uint16_t c = eot::mem::load<uint16_t>(self + kPageTyped + i * 2);
    if (c == 0)
      break;
    if (c >= 0x80)
      return std::string();
    out.push_back(static_cast<char>(c >= 'A' && c <= 'Z' ? c + 32 : c));
  }
  return out;
}

std::string PortCode() {
  std::string code = REXCVAR_GET(eot_sd_code);
  for (char &c : code)
    if (c >= 'A' && c <= 'Z')
      c = static_cast<char>(c + 32);
  return code;
}

uint32_t CallGuest(const PPCContext &ctx, uint8_t *base, PPCFunc *fn, uint32_t r3, uint32_t r4 = 0) {
  PPCContext call = ctx;
  call.r3.u32 = r3;
  call.r4.u32 = r4;
  fn(call, base);
  return call.r3.u32;
}

void AnswerWithPopup(const PPCContext &ctx, uint8_t *base, uint32_t self, uint32_t body) {
  const uint32_t config = self + kPageConfig;
  CallGuest(ctx, base, __imp__eot_TextPopup_InitConfig, config, 1);
  const uint32_t title = eot::ui::hud::FindString(ctx, base, kVipTitleNameCrc);
  const uint32_t index = eot::mem::load<uint32_t>(self + kPageTitleIndex);
  if (title && index < 6)
    eot::mem::store<uint32_t>(config + kConfigTitles + index * 4, title);
  eot::mem::store<uint8_t>(self + kPageFlags, eot::mem::load<uint8_t>(self + kPageFlags) | 4);
  eot::mem::store<uint32_t>(self + kPagePriority, 0);
  CallGuest(ctx, base, __imp__eot_TextPopup_SetBody, config, body);
  const uint32_t id = CallGuest(ctx, base, __imp__eot_YesNoWindow_Open, config);
  eot::mem::store<uint32_t>(self + kPagePopupId, id);
}

}

REX_HOOK_RAW(eot_HUDStartScreen_EnterSDCheck) {
  ForceFlag("before the start screen's check");
  __imp__eot_HUDStartScreen_EnterSDCheck(ctx, base);
}

REX_HOOK_RAW(eot_SaveGame_ApplyLoadedData) {
  __imp__eot_SaveGame_ApplyLoadedData(ctx, base);
  ForceFlag("after the save file was read");
}

REX_HOOK_RAW(eot_HUDDLCCode_Validate) {
  const uint32_t self = ctx.r3.u32;
  const std::string code = PortCode();
  if (!self || code.empty() || TypedCode(self) != code) {
    __imp__eot_HUDDLCCode_Validate(ctx, base);
    return;
  }
  if (eot::mem::load<uint8_t>(kSdSaveSeen) != 0) {
    EOT_INFO("[sd] VIP code entered again; answering 'already entered'");
    AnswerWithPopup(ctx, base, self, eot::mem::load<uint32_t>(self + kPageAlreadyHandle));
    return;
  }
  eot::mem::store<uint8_t>(kSdSaveSeen, 1);
  eot::mem::store<uint16_t>(kBonusFlags, eot::mem::load<uint16_t>(kBonusFlags) | kBonusSdWelcome);
  eot::mem::store<uint8_t>(self + kPageResult, eot::mem::load<uint8_t>(self + kPageResult) | 0x40);
  const uint32_t unlocked = eot::ui::hud::FindString(ctx, base, kSuitUnlockedNameCrc);
  if (!unlocked)
    EOT_WARN("[sd] the game's 'new Alternate Suit' string is not loaded; the answer has no body");
  AnswerWithPopup(ctx, base, self, unlocked);
  EOT_INFO("[sd] VIP code accepted: Shattered Dimensions bonus suits unlocked");
}

REX_EXTERN(__imp__eot_SM2099_Activate);              // (this r3)
REX_EXTERN(__imp__eot_SM2099HealthFX_HandleMessage); // (this r3, message r4, payload r5)

namespace {
using eot::mem::load;
using eot::mem::store;

constexpr uint32_t kCostumeResolvedMessage = 0x2DE0D24C;

constexpr uint32_t k3dObjApiPtr = 0x883CA1D4;
constexpr uint32_t kFindMaterialIndexFromCRC = 284;
constexpr uint32_t kFindMaterialAnimHandleFromCRC = 356;

constexpr uint32_t kFxTable = 0x883C29A8;
constexpr uint32_t kFxObject = kFxTable + 0;
constexpr uint32_t kFxBodyMaterial = kFxTable + 12;
constexpr uint32_t kFxBodyAnimHurt = kFxTable + 44;
constexpr uint32_t kFxBodyAnimFull = kFxTable + 48;
constexpr uint32_t kFxFlags = 0x883CF960;
constexpr uint8_t kFxFlagOn = 0x08;

constexpr uint32_t kHeroParamBlock = 14596;
constexpr uint32_t kShieldDataOffset = 832;
constexpr uint32_t kRecordBodyMaterial = 6 * 4;
constexpr uint32_t kRecordBodyAnimHurt = 12 * 4;
constexpr uint32_t kRecordBodyAnimFull = 15 * 4;

constexpr uint32_t kNone = 0xFFFFFFFFu;

uint32_t ApiCall(const PPCContext &ctx, uint8_t *base, uint32_t slot, uint32_t r3, uint32_t r4, uint32_t r5,
                 uint32_t r6 = 0) {
  const uint32_t table = load<uint32_t>(k3dObjApiPtr);
  if (!table)
    return kNone;
  return eot::ui::hud::CallAt(ctx, base, load<uint32_t>(table + slot), r3, r4, r5, r6);
}

uint32_t ShieldDataRecord(uint32_t self) {
  const uint32_t block = load<uint32_t>(self + kHeroParamBlock);
  const uint32_t row = block ? load<uint32_t>(block) : 0;
  if (!row)
    return 0;
  return row + kShieldDataOffset + load<uint16_t>(row + kShieldDataOffset);
}

uint32_t g_bodyMaterialCrc = 0;

bool TableIsLive(const PPCContext &ctx, uint8_t *base) {
  const uint32_t object = load<uint32_t>(kFxObject);
  const uint32_t material = load<uint32_t>(kFxBodyMaterial);
  if (!g_bodyMaterialCrc || object == 0 || object == kNone || material == kNone)
    return false;
  return ApiCall(ctx, base, kFindMaterialIndexFromCRC, object, 0, g_bodyMaterialCrc) == material;
}
}

REX_HOOK_RAW(eot_SM2099_Activate) {
  const uint32_t self = ctx.r3.u32;
  __imp__eot_SM2099_Activate(ctx, base);
  const uint32_t object = load<uint32_t>(kFxObject);
  const uint32_t record = ShieldDataRecord(self);
  if (!record || object == 0 || object == kNone)
    return;
  g_bodyMaterialCrc = load<uint32_t>(record + kRecordBodyMaterial);
  if (load<uint32_t>(kFxBodyMaterial) != kNone)
    return;
  const uint32_t material = ApiCall(ctx, base, kFindMaterialIndexFromCRC, object, 0, g_bodyMaterialCrc);
  if (material == kNone) {
    store<uint8_t>(kFxFlags, load<uint8_t>(kFxFlags) & static_cast<uint8_t>(~kFxFlagOn));
    return;
  }
  const uint32_t hurt = ApiCall(ctx, base, kFindMaterialAnimHandleFromCRC, object, 0, material, load<uint32_t>(record + kRecordBodyAnimHurt));
  const uint32_t full = ApiCall(ctx, base, kFindMaterialAnimHandleFromCRC, object, 0, material, load<uint32_t>(record + kRecordBodyAnimFull));
  store<uint32_t>(kFxBodyMaterial, material);
  store<uint32_t>(kFxBodyAnimHurt, hurt);
  store<uint32_t>(kFxBodyAnimFull, full);
  store<uint8_t>(kFxFlags, load<uint8_t>(kFxFlags) | kFxFlagOn);
  EOT_DEBUG("[suit] 2099 body pulse on a custom suit: object {:#x} material {} animations {:#x} / {:#x}", object, material, full, hurt);
}

REX_HOOK_RAW(eot_SM2099HealthFX_HandleMessage) {
  const uint32_t message = ctx.r4.u32;
  __imp__eot_SM2099HealthFX_HandleMessage(ctx, base);
  if (message == kCostumeResolvedMessage && TableIsLive(ctx, base))
    store<uint8_t>(kFxFlags, load<uint8_t>(kFxFlags) | kFxFlagOn);
}

REX_EXTERN(__imp__eot_GLInstanciateGenericSectionControl); // (params r3, out r4, out r5) -> control r3

namespace {
using eot::mem::load;
using eot::mem::store;

constexpr uint32_t kControlSectionId = 40;
constexpr uint32_t kControlParams = 44;
constexpr uint32_t kParamsLightManager = 20;

struct InheritedLights {
  uint32_t section;
  uint32_t light_manager;
};

constexpr uint32_t kLightManagerSession5 = 0x338E6638;
constexpr InheritedLights kInherited[] = {
    {32, kLightManagerSession5},
    {33, kLightManagerSession5},
    {34, kLightManagerSession5},
    {35, kLightManagerSession5},
    {42, kLightManagerSession5},
};
}

REX_HOOK_RAW(eot_GLInstanciateGenericSectionControl) {
  __imp__eot_GLInstanciateGenericSectionControl(ctx, base);
  const uint32_t control = ctx.r3.u32;
  if (!control)
    return;
  const uint32_t params = load<uint32_t>(control + kControlParams);
  if (!params || load<uint32_t>(params + kParamsLightManager) != kNone)
    return;
  const uint32_t section = load<uint32_t>(control + kControlSectionId);
  for (const auto &entry : kInherited) {
    if (entry.section != section)
      continue;
    store<uint32_t>(params + kParamsLightManager, entry.light_manager);
    EOT_DEBUG("[lights] section {} takes LightManager {:#010x} (the one the sections before it applied)", section,
              entry.light_manager);
    return;
  }
}
