#include "Game.h"
#include "apex_sky.h"
#include "lib/xorstr/xorstr.hpp"
#include "vector.h"
#include <array>
#include <cassert>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <ostream>
#include <thread>

extern Memory apex_mem;
extern const exported_offsets_t offsets;

float bulletspeed = 0.08;
float bulletgrav = 0.05;

// glowtype not used, but dont delete its still used.
extern int glowtype;
extern int glowtype2;
// setting up vars, dont edit
extern float veltest;
extern Vector aim_target;

bool Entity::Observing(uint64_t entitylist) {
  return *(bool *)(buffer + offsets.player_observer_state);
}

// https://github.com/CasualX/apexbot/blob/master/src/state.cpp#L104
void get_class_name(uint64_t entity_ptr, char *out_str) {
  uint64_t client_networkable_vtable;
  apex_mem.Read<uint64_t>(entity_ptr + 8 * 3, client_networkable_vtable);

  uint64_t get_client_class;
  apex_mem.Read<uint64_t>(client_networkable_vtable + 8 * 3, get_client_class);

  uint32_t disp;
  apex_mem.Read<uint32_t>(get_client_class + 3, disp);
  const uint64_t client_class_ptr = get_client_class + disp + 7;

  ClientClass client_class;
  apex_mem.Read<ClientClass>(client_class_ptr, client_class);

  apex_mem.ReadArray<char>(client_class.pNetworkName, out_str, 32);
}

int Entity::getTeamId() { return *(int *)(buffer + offsets.entity_team_num); }

int Entity::getHealth() {
  assert(this->is_player);
  return *(int *)(buffer + offsets.player_health);
}

int Entity::getArmortype() {
  assert(this->is_player);
  int armortype;
  apex_mem.Read<int>(ptr + offsets.player_armortype, armortype);
  return armortype;
}

int Entity::getShield() {
  return *(int *)(buffer + offsets.entity_shieldhealth);
}

int Entity::getMaxshield() {
  return *(int *)(buffer + offsets.entity_maxshieldhealth);
}

Vector Entity::getAbsVelocity() {
  return *(Vector *)(buffer + offsets.centity_abs_velocity);
}

Vector Entity::getPosition() {
  return *(Vector *)(buffer + offsets.centity_origin);
}
Vector Entity::getViewOffset() {
  return *(Vector *)(buffer + offsets.centity_viewoffset);
}

bool Entity::isPlayer(uintptr_t ptr) {
  // char class_name[33] = {};
  // get_class_name(ptr, class_name);
  uint64_t entity_name;
  apex_mem.Read(ptr + offsets.entiry_name, entity_name);
  bool r = entity_name == 125780153691248;
  // if (r) {
  //   printf("isPlayer %s %d\n", class_name, r);
  // }
  return r;
}
// firing range dummys
bool Entity::isDummy(uintptr_t ptr) {
  char class_name[33] = {};
  get_class_name(ptr, class_name);

  return strncmp(class_name, xorstr_("CAI_BaseNPC"), 11) == 0;
}

bool Entity::isKnocked() {
  assert(this->is_player);
  return *(int *)(buffer + offsets.player_bleed_out_state) > 0;
}

bool Entity::isAlive() {
  assert(this->is_player);
  return *(int *)(buffer + offsets.player_life_state) == 0;
}

float Entity::lastVisTime() {
  return *(float *)(buffer + offsets.player_last_visible_time);
}

float Entity::lastCrossHairTime() {
  return *(float *)(buffer + offsets.cweaponx_crosshair_last);
}

Vector Entity::getBonePositionByHitbox(int id) {
  Vector origin = getPosition();

  // BoneByHitBox
  uint64_t Model = *(uint64_t *)(buffer + offsets.animating_studiohdr);

  // get studio hdr
  uint64_t StudioHdr;
  apex_mem.Read<uint64_t>(Model + 0x8, StudioHdr);

  // get hitbox array
  uint16_t HitboxCache;
  apex_mem.Read<uint16_t>(StudioHdr + 0x34, HitboxCache);
  uint64_t HitboxArray =
      StudioHdr + ((uint16_t)(HitboxCache & 0xFFFE) << (4 * (HitboxCache & 1)));

  uint16_t IndexCache;
  apex_mem.Read<uint16_t>(HitboxArray + 0x4, IndexCache);
  int HitboxIndex = ((uint16_t)(IndexCache & 0xFFFE) << (4 * (IndexCache & 1)));

  uint16_t Bone;
  apex_mem.Read<uint16_t>(HitboxIndex + HitboxArray + (id * 0x20), Bone);

  if (Bone < 0 || Bone > 255)
    return Vector();

  // hitpos
  uint64_t Bones = *(uint64_t *)(buffer + offsets.bones);

  matrix3x4_t Matrix = {};
  apex_mem.Read<matrix3x4_t>(Bones + Bone * sizeof(matrix3x4_t), Matrix);

  return Vector(Matrix.m_flMatVal[0][3] + origin.x,
                Matrix.m_flMatVal[1][3] + origin.y,
                Matrix.m_flMatVal[2][3] + origin.z);
}

QAngle Entity::GetSwayAngles() {
  return *(QAngle *)(buffer + offsets.player_breath_angles);
}

QAngle Entity::GetViewAngles() {
  assert(this->is_player);
  return *(QAngle *)(buffer + offsets.player_viewangles);
}

Vector Entity::GetViewAnglesV() {
  assert(this->is_player);
  return *(Vector *)(buffer + offsets.player_viewangles);
}

float Entity::GetYaw() {
  float yaw = 0;
  apex_mem.Read<float>(ptr + OFFSET_YAW, yaw);

  if (yaw < 0)
    yaw += 360;
  yaw += 90;
  if (yaw > 360)
    yaw -= 360;

  return yaw;
}

bool Entity::isGlowing() {
  return *(uint8_t *)(buffer + OFFSET_GLOW_CONTEXT_ID) == 7;
}

bool Entity::isZooming() {
  assert(this->is_player);
  return *(int *)(buffer + offsets.player_zooming) == 1;
}

extern uint64_t g_Base;

void Entity::enableGlow(int setting_index, uint8_t inside_value,
                        uint8_t outline_size,
                        std::array<float, 3> highlight_color) {

  const unsigned char outsidevalue = 125;

  HighlightSetting_t highlight_settings;
  highlight_settings.inner_function = inside_value; // InsideFunction
  highlight_settings.outside_function =
      outsidevalue; // OutlineFunction: HIGHLIGHT_OUTLINE_OBJECTIVE
  highlight_settings.outside_radius =
      outline_size; // OutlineRadius: size * 255 / 8
  highlight_settings.state = 0;
  highlight_settings.shouldDraw = 1;
  highlight_settings.postProcess = 0;
  highlight_settings.color1[0] = highlight_color[0];
  highlight_settings.color1[1] = highlight_color[1];
  highlight_settings.color1[2] = highlight_color[2];

  uint8_t context_id = setting_index;
  apex_mem.Write<uint8_t>(ptr + OFFSET_GLOW_CONTEXT_ID, context_id);
  apex_mem.Write<int>(ptr + GLOW_VISIBLE_TYPE, 2);

  long highlight_settings_ptr;
  apex_mem.Read<long>(g_Base + offsets.highlight_settings,
                      highlight_settings_ptr);
  apex_mem.Write<HighlightSetting_t>(highlight_settings_ptr + 0x34 * context_id,
                                     highlight_settings);

  apex_mem.Write<float>(ptr + 0x264, 8.0E+4);

  apex_mem.Write(g_Base + OFFSET_GLOW_FIX, 1);
}

void Entity::disableGlow() {
  uint8_t context_id = *(uint8_t *)(this->buffer + OFFSET_GLOW_CONTEXT_ID);
  if (context_id >= 80 && context_id < 100) {
    apex_mem.Write<uint8_t>(this->ptr + OFFSET_GLOW_CONTEXT_ID, 0);
  }
}

void Entity::SetViewAngles(SVector angles) {
  assert(this->is_player);
  apex_mem.Write<SVector>(ptr + offsets.player_viewangles, angles);
}

void Entity::SetViewAngles(QAngle &angles) {
  this->SetViewAngles(SVector(angles));
}

Vector Entity::GetCamPos() {
  assert(this->is_player);
  return *(Vector *)(buffer + offsets.cplayer_camerapos);
}

QAngle Entity::GetRecoil() {
  assert(this->is_player);
  return *(QAngle *)(buffer + offsets.cplayer_aimpunch);
}

void Entity::get_name(char *name) {
  uint64_t index = (this->entity_index - 1) * 24;
  uint64_t name_ptr = 0;
  apex_mem.Read<uint64_t>(g_Base + offsets.name_list + index, name_ptr);
  apex_mem.ReadArray<char>(name_ptr, name, 32);
}

void Entity::glow_weapon_model(bool enable_glow, bool enable_draw,
                               std::array<float, 3> highlight_color) {
  assert(this->is_player);

  uint64_t view_model_handle;
  apex_mem.Read<uint64_t>(ptr + offsets.cplayer_viewmodels, view_model_handle);
  view_model_handle &= 0xFFFF;
  uint64_t view_model_ptr = 0;
  apex_mem.Read<uint64_t>(
      g_Base + offsets.entitylist + (view_model_handle << 5), view_model_ptr);

  // printf("view model handle=%lu, ptr=%lu, \n", view_model_handle,
  // view_model_ptr);

  // uint64_t name_ptr;
  // char name_str[200];
  // apex_mem.Read<uint64_t>(view_model_ptr + offsets.offset_centity_modelname,
  // name_ptr); apex_mem.ReadArray<char>(name_ptr, name_str, 200);
  // printf("name=%s\n", name_str);

  std::array<unsigned char, 4> highlightFunctionBits = {0, 125, 64, 64};
  if (!enable_glow) {
    apex_mem.Write<uint8_t>(view_model_ptr + OFFSET_GLOW_CONTEXT_ID, 0);
    return;
  }

  HighlightSetting_t highlight_settings;
  highlight_settings.inner_function =
      highlightFunctionBits[0]; // InsideFunction
  highlight_settings.outside_function =
      highlightFunctionBits[1]; // OutlineFunction: HIGHLIGHT_OUTLINE_OBJECTIVE
  highlight_settings.outside_radius =
      highlightFunctionBits[2]; // OutlineRadius: size * 255 / 8
  highlight_settings.state = 0;
  highlight_settings.shouldDraw = enable_draw;
  highlight_settings.postProcess = 0;
  highlight_settings.color1[0] = highlight_color[0];
  highlight_settings.color1[1] = highlight_color[1];
  highlight_settings.color1[2] = highlight_color[2];

  long highlight_settings_ptr;
  apex_mem.Read<long>(g_Base + offsets.highlight_settings,
                      highlight_settings_ptr);

  uint8_t context_id = 99;
  apex_mem.Write<uint8_t>(view_model_ptr + OFFSET_GLOW_CONTEXT_ID, context_id);
  apex_mem.Write<HighlightSetting_t>(highlight_settings_ptr + 0x34 * context_id,
                                     highlight_settings);
}

LoveStatus Entity::check_love_player() {
  if (global_settings().yuan_p) {
    if (Entity::isDummy(this->ptr))
      return LOVE;
  } else {
    if (!this->is_player)
      return LoveStatus::NORMAL;
  }
  uint64_t data_fid[4];
  data_fid[0] = *((uint64_t *)(buffer + offsets.player_platform_uid + 0));
  data_fid[1] = *((uint64_t *)(buffer + offsets.player_platform_uid + 4));
  data_fid[2] = *((uint64_t *)(buffer + offsets.player_platform_uid + 16));
  data_fid[3] = *((uint64_t *)(buffer + offsets.player_platform_uid + 20));
  uint64_t platform_lid = data_fid[0] | data_fid[1] << 32;
  uint64_t eadp_lid = data_fid[1] | data_fid[2] << 32;
  char name[33] = {0};
  this->get_name(&name[0]);
  // printf("check love: %s\n", name);
  return ::check_love_player(platform_lid, eadp_lid, name, this->ptr);
}

int Entity::xp_level() {
  assert(this->is_player);
  return this->player_xp_level + 1;
}

int Entity::read_xp_level() {
  assert(this->is_player);

  int xp = 0;
  apex_mem.Read<int>(this->ptr + offsets.player_xp, xp);

  /*
    MIT License

    Copyright (c) 2023 Xnieno

    Permission is hereby granted, free of charge, to any person obtaining a copy
    of this software and associated documentation files (the "Software"), to
    deal in the Software without restriction, including without limitation the
    rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
    sell copies of the Software, and to permit persons to whom the Software is
    furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included in
    all copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
    FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
    IN THE SOFTWARE.
  */
  static int levels[] = {2750,   6650,   11400,  17000,  23350,  30450,  38300,
                         46450,  55050,  64100,  73600,  83550,  93950,  104800,
                         116100, 127850, 140050, 152400, 164900, 177550, 190350,
                         203300, 216400, 229650, 243050, 256600, 270300, 284150,
                         298150, 312300, 326600, 341050, 355650, 370400, 385300,
                         400350, 415550, 430900, 446400, 462050, 477850, 493800,
                         509900, 526150, 542550, 559100, 575800, 592650, 609650,
                         626800, 644100, 661550, 679150, 696900, 714800};

  if (xp < 0)
    return 0;
  if (xp < 100)
    return 1;

  int level = 56;
  int arraySize = sizeof(levels) / sizeof(levels[0]);

  for (int i = 0; i < arraySize; i++) {
    if (xp < levels[i]) {
      return i + 1;
    }
  }

  return level + ((xp - levels[arraySize - 1] + 1) / 18000);
}

// Items
bool Item::isItem() {
  char class_name[33] = {};
  get_class_name(ptr, class_name);

  return strncmp(class_name, xorstr_("CPropSurvival"), 13) == 0;
}
// Deathboxes
bool Item::isBox() {
  char class_name[33] = {};
  get_class_name(ptr, class_name);

  return strncmp(class_name, xorstr_("CDeathBoxProp"), 13) == 0;
}
// Traps
bool Item::isTrap() {
  char class_name[33] = {};
  get_class_name(ptr, class_name);

  return strncmp(class_name, xorstr_("caustic_trap"), 13) == 0;
}

// bool Item::isGlowing() {
//   return *(int *)(buffer + OFFSET_ITEM_GLOW) == 1363184265;
// }

void Item::enableGlow(std::array<unsigned char, 4> highlightFunctionBits,
                      std::array<float, 3> highlightParameter,
                      int settingIndex) {
  HighlightSetting_t highlight_settings;
  highlight_settings.inner_function =
      highlightFunctionBits[0]; // InsideFunction
  highlight_settings.outside_function =
      highlightFunctionBits[1]; // OutlineFunction: HIGHLIGHT_OUTLINE_OBJECTIVE
  highlight_settings.outside_radius =
      highlightFunctionBits[2]; // OutlineRadius: size * 255 / 8
  highlight_settings.state = 0;
  highlight_settings.shouldDraw = 1;
  highlight_settings.postProcess = 0;
  highlight_settings.color1[0] = highlightParameter[0];
  highlight_settings.color1[1] = highlightParameter[1];
  highlight_settings.color1[2] = highlightParameter[2];

  long highlight_settings_ptr;
  apex_mem.Read<long>(g_Base + offsets.highlight_settings,
                      highlight_settings_ptr);

  uint8_t context_id = settingIndex;
  apex_mem.Write<uint8_t>(this->ptr + OFFSET_GLOW_CONTEXT_ID, context_id);
  apex_mem.Write<HighlightSetting_t>(highlight_settings_ptr + 0x34 * context_id,
                                     highlight_settings);
}

// void Item::disableGlow() {
//   apex_mem.Write<int>(ptr + OFFSET_GLOW_ENABLE, 0);
//   apex_mem.Write<int>(ptr + OFFSET_HIGHLIGHTSERVERACTIVESTATES + 0, 0);
//   apex_mem.Write<int>(ptr + OFFSET_GLOW_THROUGH_WALLS_GLOW_VISIBLE_TYPE, 5);
// }

Vector Item::getPosition() {
  return *(Vector *)(buffer + offsets.centity_origin);
}

float CalculateFov(Entity &from, Entity &target) {
  QAngle ViewAngles = from.GetSwayAngles();
  Vector LocalCamera = from.GetCamPos();
  Vector EntityPosition = target.getBonePositionByHitbox(2);
  QAngle Angle = Math::CalcAngle(LocalCamera, EntityPosition);
  return Math::GetFov(ViewAngles, Angle);
}

auto fun_calc_angles = [](Vector LocalCameraPosition, Vector TargetBonePosition,
                          Vector targetVel, float BulletSpeed, float BulletGrav,
                          float deltaTime) {
  QAngle CalculatedAngles;
  if (BulletSpeed > 1.f) {
    float distanceToTarget =
        (TargetBonePosition - LocalCameraPosition).Length();
    float timeToTarget = distanceToTarget / BulletSpeed;
    Vector targetPosAhead = TargetBonePosition + (targetVel * timeToTarget);
    // // Add the target's velocity to the prediction context, with an offset
    // // in the y direction
    // Ctx.TargetVel =
    //     Vector(targetVel.x, targetVel.y + (targetVel.Length() * deltaTime),
    //            targetVel.z);

    aim_target = targetPosAhead;

    vec4_t result = linear_predict(
        BulletGrav, BulletSpeed, LocalCameraPosition.x, LocalCameraPosition.y,
        LocalCameraPosition.z, targetPosAhead.x, targetPosAhead.y,
        targetPosAhead.z, targetVel.x, targetVel.y, targetVel.z);
    if (result.w != 0) {
      CalculatedAngles = QAngle{result.x, result.y, 0.f};
      // printf("%f, %f \n", CalculatedAngles.x, CalculatedAngles.y);
    }
  } else {
    CalculatedAngles = Math::CalcAngle(LocalCameraPosition, TargetBonePosition);
  }
  return CalculatedAngles;
};

aim_angles_t CalculateBestBoneAim(Entity &from, Entity &target,
                                  const aimbot_state_t &aimbot) {
  QAngle ViewAngles = from.GetViewAngles();
  Vector LocalCamera = from.GetCamPos();
  QAngle SwayAngles = from.GetSwayAngles();
  Vector targetVel = target.getAbsVelocity();
  float distance = LocalCamera.DistTo(target.getPosition());

  Vector TargetBonePositionMin;
  Vector TargetBonePositionMax;

  // Calculate the time since the last frame (in seconds)
  float deltaTime = 1.0 / aimbot.game_fps;

  if (aimbot.weapon_info.weapon_headshot &&
      distance <= aimbot.settings.headshot_dist) {
    TargetBonePositionMax = TargetBonePositionMin =
        target.getBonePositionByHitbox(0);
  } else if (aimbot.settings.bone_nearest) {
    // find nearest bone
    float NearestBoneDistance = aimbot.settings.max_dist;
    for (int i = 0; i < 4; i++) {
      Vector currentBonePosition = target.getBonePositionByHitbox(i);
      float DistanceFromCrosshair =
          (currentBonePosition - LocalCamera).Length();
      if (DistanceFromCrosshair < NearestBoneDistance) {
        TargetBonePositionMax = TargetBonePositionMin = currentBonePosition;
        NearestBoneDistance = DistanceFromCrosshair;
      }
    }
  } else if (aimbot.settings.bone_auto) {
    TargetBonePositionMax = target.getPosition();
    TargetBonePositionMin = target.getBonePositionByHitbox(0);
  } else {
    TargetBonePositionMax = TargetBonePositionMin =
        target.getBonePositionByHitbox(aimbot.settings.bone);
  }

  if (!aimbot.held_grenade) {
    QAngle CalculatedAnglesMin =
        fun_calc_angles(LocalCamera, TargetBonePositionMin, targetVel,
                        aimbot.weapon_info.bullet_speed,
                        aimbot.weapon_info.bullet_gravity, deltaTime);
    QAngle CalculatedAnglesMax =
        fun_calc_angles(LocalCamera, TargetBonePositionMax, targetVel,
                        aimbot.weapon_info.bullet_speed,
                        aimbot.weapon_info.bullet_gravity, deltaTime);

    double fov0 = Math::GetFov(SwayAngles, CalculatedAnglesMin);
    double fov1 = Math::GetFov(SwayAngles, CalculatedAnglesMax);
    float max_fov = aimbot.max_fov;
    float zoom_fov = aimbot.weapon_info.weapon_zoom_fov;
    if (zoom_fov != 0.0f && zoom_fov != 1.0f) {
      max_fov *= zoom_fov / 90.0f;
    }
    if ((fov0 + fov1) * 0.5f > max_fov) {
      return aim_angles_t{false};
    }
    CalculatedAnglesMin -= SwayAngles - ViewAngles;
    CalculatedAnglesMax -= SwayAngles - ViewAngles;
    Math::NormalizeAngles(CalculatedAnglesMin);
    Math::NormalizeAngles(CalculatedAnglesMax);
    QAngle DeltaMin = CalculatedAnglesMin - ViewAngles;
    QAngle DeltaMax = CalculatedAnglesMax - ViewAngles;
    Math::NormalizeDeltaAngles(DeltaMin);
    Math::NormalizeDeltaAngles(DeltaMax);

    QAngle Delta = QAngle(0, 0, 0);
    if (DeltaMin.x * DeltaMax.x > 0)
      Delta.x = (DeltaMin.x + DeltaMax.x) * 0.5f;
    if (DeltaMin.y * DeltaMax.y > 0)
      Delta.y = (DeltaMin.y + DeltaMax.y) * 0.5f;

    return aim_angles_t{true,       ViewAngles.x, ViewAngles.y, Delta.x,
                        Delta.y,    DeltaMin.x,   DeltaMax.x,   DeltaMin.y,
                        DeltaMax.y, distance};
  } else {
    Vector local_origin = from.getPosition();
    Vector view_offset = from.getViewOffset();
    // printf("view_offset(%f,%f,%f)\n", view_offset.x, view_offset.y,
    //        view_offset.z);
    Vector view_origin = local_origin + view_offset;
    Vector target_origin = target.getPosition() + targetVel * deltaTime;
    aim_target = target_origin;

    QAngle target_angle = Math::CalcAngle(view_origin, target_origin);
    if (abs(target_angle.x) > 80) {
      return aim_angles_t{false};
    }

    vec4_t skynade_angles = skynade_angle(
        aimbot.weapon_info.weapon_id, aimbot.weapon_info.weapon_mod_bitfield,
        aimbot.weapon_info.bullet_gravity / 750.0f,
        aimbot.weapon_info.bullet_speed, view_origin.x, view_origin.y,
        view_origin.z, target_origin.x, target_origin.y, target_origin.z);

    // printf("(%.1f, %.1f)\n", ViewAngles.x, ViewAngles.y);

    // printf("skynade: (%f,%f,%f,%f)\n", skynade_angles.x, skynade_angles.y,
    //        skynade_angles.z, skynade_angles.w);
    if (skynade_angles.w == 0) {
      return aim_angles_t{false};
    }

    const float PIS_IN_180 = 57.2957795130823208767981548141051703f;
    QAngle target_aim_angles = QAngle(-skynade_angles.x * PIS_IN_180,
                                      skynade_angles.y * PIS_IN_180, 0);
    // printf("weap=%d, bitfield=%d, (%.1f, %.1f)\n", weapon_id,
    //        weapon_mod_bitfield, TargetAngles.x, TargetAngles.y);

    QAngle Delta = target_aim_angles - ViewAngles;
    Math::NormalizeDeltaAngles(Delta);
    return aim_angles_t{true,    ViewAngles.x, ViewAngles.y, Delta.x, Delta.y,
                        Delta.x, Delta.x,      Delta.y,      Delta.y, distance};
  }
}

Entity getEntity(uintptr_t ptr) {
  Entity entity = Entity();
  entity.ptr = ptr;
  apex_mem.ReadArray<uint8_t>(ptr, entity.buffer, sizeof(entity.buffer));
  entity.entity_index = *(uint64_t *)(entity.buffer + 0x38);
  if (Entity::isPlayer(ptr)) {
    entity.is_player = true;
    entity.player_xp_level = entity.read_xp_level();
  }
  return entity;
}

Item getItem(uintptr_t ptr) {
  Item entity = Item();
  entity.ptr = ptr;
  apex_mem.ReadArray<uint8_t>(ptr, entity.buffer, sizeof(entity.buffer));
  return entity;
}

bool WorldToScreen(Vector from, float *m_vMatrix, int targetWidth,
                   int targetHeight, Vector &to) {
  float w = m_vMatrix[12] * from.x + m_vMatrix[13] * from.y +
            m_vMatrix[14] * from.z + m_vMatrix[15];

  if (w < 0.01f)
    return false;

  to.x = m_vMatrix[0] * from.x + m_vMatrix[1] * from.y + m_vMatrix[2] * from.z +
         m_vMatrix[3];
  to.y = m_vMatrix[4] * from.x + m_vMatrix[5] * from.y + m_vMatrix[6] * from.z +
         m_vMatrix[7];

  float invw = 1.0f / w;
  to.x *= invw;
  to.y *= invw;

  float x = targetWidth / 2.0;
  float y = targetHeight / 2.0;

  x += 0.5 * to.x * targetWidth + 0.5;
  y -= 0.5 * to.y * targetHeight + 0.5;

  to.x = x;
  to.y = y;
  to.z = 0;
  return true;
}

void WeaponXEntity::update(uint64_t LocalPlayer) {
  extern uint64_t g_Base;
  uint64_t entitylist = g_Base + offsets.entitylist;
  uint64_t wephandle = 0;
  apex_mem.Read<uint64_t>(LocalPlayer + offsets.bcc_active_weapon, wephandle);

  wephandle &= 0xffff;

  uint64_t wep_entity = 0;
  apex_mem.Read<uint64_t>(entitylist + (wephandle << 5), wep_entity);

  projectile_speed = 0;
  apex_mem.Read<float>(wep_entity + offsets.weaponx_projectile_launch_speed,
                       projectile_speed);
  projectile_scale = 0;
  apex_mem.Read<float>(wep_entity + offsets.weaponx_projectile_gravity_scale,
                       projectile_scale);
  zoom_fov = 0;
  apex_mem.Read<float>(wep_entity + offsets.weaponx_zoom_fov, zoom_fov);
  ammo = 0;
  apex_mem.Read<int>(wep_entity + offsets.weaponx_ammo_in_clip, ammo);
  memset(name_str, 0, sizeof(name_str));
  uint64_t name_ptr;
  apex_mem.Read<uint64_t>(wep_entity + offsets.centity_modelname, name_ptr);
  apex_mem.ReadArray<char>(name_ptr, name_str, 200);
  mod_bitfield = 0;
  apex_mem.Read<int>(wep_entity + offsets.weaponx_bitfield_from_player,
                     mod_bitfield);
  weap_id = 0;
  apex_mem.Read<uint32_t>(wep_entity + offsets.weaponx_weapon_name_index,
                          weap_id);
  lastChargeLevel = 0;
  apex_mem.Read<int>(wep_entity + m_lastChargeLevel, lastChargeLevel);
}

float WeaponXEntity::get_projectile_speed() {
  return ((weap_id == 2)
              ? projectile_speed + pow(lastChargeLevel, 5.4684388195808)
              : projectile_speed);
}

float WeaponXEntity::get_projectile_gravity() {
  return 750.0f * projectile_scale;
}

float WeaponXEntity::get_zoom_fov() { return zoom_fov; }

int WeaponXEntity::get_ammo() { return ammo; }

const char *WeaponXEntity::get_name_str() { return name_str; }

int WeaponXEntity::get_mod_bitfield() { return mod_bitfield; }

uint32_t WeaponXEntity::get_weap_id() { return weap_id; }
