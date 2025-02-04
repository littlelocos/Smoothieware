#include "TripodDeltaSolution.h"
#include "ActuatorCoordinates.h"
#include "checksumm.h"
#include "ConfigValue.h"
#include "libs/Kernel.h"
#include "libs/nuts_bolts.h"
#include "libs/Config.h"

#include <fastmath.h>
    // #include "Vector3.h" (TWS - vector math set not needed)


    // #define arm_length_checksum         CHECKSUM("arm_length")  (TWS - replace arm_length_checksum with pivot_height_checksum for tripodkins)
#define pivot_height_checksum       CHECKSUM("pivot_height")
#define arm_radius_checksum         CHECKSUM("arm_radius")

    // change tower(i)_offset to effector(i)_offset as no tower offset needed in tripodkins
    // effector(i)_offset is distance between hinges and center of effector
#define effector1_offset_checksum      CHECKSUM("tripod_effector1_offset")
#define effector2_offset_checksum      CHECKSUM("tripod_effector2_offset")
#define effector3_offset_checksum      CHECKSUM("tripod_effector3_offset")
#define tower1_angle_checksum       CHECKSUM("delta_tower1_angle")
#define tower2_angle_checksum       CHECKSUM("delta_tower2_angle")
#define tower3_angle_checksum       CHECKSUM("delta_tower3_angle")

#define SQ(x) powf(x, 2)
#define ROUND(x, y) (roundf(x * (float)(1e ## y)) / (float)(1e ## y))
#define PIOVER180   0.01745329251994329576923690768489F

TripodDeltaSolution::TripodDeltaSolution(Config* config)
{
    // arm_length is the length of the arm from hinge to hinge
    // arm_length = config->value(arm_length_checksum)->by_default(250.0f)->as_number();

    // pivot_height is the vertical distance between the hinges at the effector and the pivot points in the structure when the nozzle is at Z=0.00 
    pivot_height = config->value(pivot_height_checksum)->by_default(250.0f)->as_number();

    // arm_radius is the horizontal distance from hinge to hinge when the effector is centered
    arm_radius = config->value(arm_radius_checksum)->by_default(124.0f)->as_number();

    tower1_angle = config->value(tower1_angle_checksum)->by_default(0.0f)->as_number();
    tower2_angle = config->value(tower2_angle_checksum)->by_default(0.0f)->as_number();
    tower3_angle = config->value(tower3_angle_checksum)->by_default(0.0f)->as_number();
    effector1_offset = config->value(effector1_offset_checksum)->by_default(25.0f)->as_number();
    effector2_offset = config->value(effector2_offset_checksum)->by_default(25.0f)->as_number();
    effector3_offset = config->value(effector3_offset_checksum)->by_default(25.0f)->as_number();

    init();
}

void TripodDeltaSolution::init()
{
    // arm_length_squared = SQ(arm_length);  (TWS - Not needed in tripod kins)

    // Effective X/Y positions of the three towers.  For tripod kins, this is the position of the pivot points at the top of the structure.
    float delta_radius = arm_radius;

    delta_tower1_x = (delta_radius + effector1_offset) * cosf((210.0F + tower1_angle) * PIOVER180); // front left tower
    delta_tower1_y = (delta_radius + effector1_offset) * sinf((210.0F + tower1_angle) * PIOVER180);
    delta_tower1_z = pivot_height;
    delta_tower2_x = (delta_radius + effector2_offset) * cosf((330.0F + tower2_angle) * PIOVER180); // front right tower
    delta_tower2_y = (delta_radius + effector2_offset) * sinf((330.0F + tower2_angle) * PIOVER180);
    delta_tower2_z = pivot_height;
    delta_tower3_x = (delta_radius + effector3_offset) * cosf((90.0F  + tower3_angle) * PIOVER180); // back middle tower
    delta_tower3_y = (delta_radius + effector3_offset) * sinf((90.0F  + tower3_angle) * PIOVER180);
    delta_tower3_z = pivot_height;
}
    // INVERSE KINEMATIC EQUATIONS

void TripodDeltaSolution::cartesian_to_actuator(const float cartesian_mm[], ActuatorCoordinates &actuator_mm ) const
{

    actuator_mm[ALPHA_STEPPER] = sqrtf(this->(SQ(delta_tower1_x - cartesian_mm[X_AXIS])
                                       + SQ(delta_tower1_y - cartesian_mm[Y_AXIS])
                                       + SQ(delta_tower1_z - cartesian_mm[Z_AXIS]));

    actuator_mm[BETA_STEPPER ] = sqrtf(this->(SQ(delta_tower2_x - cartesian_mm[X_AXIS])
                                       + SQ(delta_tower2_y - cartesian_mm[Y_AXIS])
                                       + SQ(delta_tower2_z - cartesian_mm[Z_AXIS]));

    actuator_mm[GAMMA_STEPPER] = sqrtf(this->(SQ(delta_tower3_x - cartesian_mm[X_AXIS])
                                       + SQ(delta_tower3_y - cartesian_mm[Y_AXIS])
                                       + SQ(delta_tower3_z - cartesian_mm[Z_AXIS]));

}

    // FORWARD KINEMATIC EQUATIONS

void TripodDeltaSolution::actuator_to_cartesian(const ActuatorCoordinates &actuator_mm, float cartesian_mm[] ) const
{
    // from http://en.wikipedia.org/wiki/Circumscribed_circle#Barycentric_coordinates_from_cross-_and_dot-products
    // based on https://github.com/ambrop72/aprinter/blob/2de69a/aprinter/printer/DeltaTransform.h#L81
    Vector3 tower1( delta_tower1_x, delta_tower1_y, actuator_mm[0] );
    Vector3 tower2( delta_tower2_x, delta_tower2_y, actuator_mm[1] );
    Vector3 tower3( delta_tower3_x, delta_tower3_y, actuator_mm[2] );

    Vector3 s12 = tower1.sub(tower2);
    Vector3 s23 = tower2.sub(tower3);
    Vector3 s13 = tower1.sub(tower3);

    Vector3 normal = s12.cross(s23);

    float magsq_s12 = s12.magsq();
    float magsq_s23 = s23.magsq();
    float magsq_s13 = s13.magsq();

    float inv_nmag_sq = 1.0F / normal.magsq();
    float q = 0.5F * inv_nmag_sq;

    float a = q * magsq_s23 * s12.dot(s13);
    float b = q * magsq_s13 * s12.dot(s23) * -1.0F; // negate because we use s12 instead of s21
    float c = q * magsq_s12 * s13.dot(s23);

    Vector3 circumcenter( delta_tower1_x * a + delta_tower2_x * b + delta_tower3_x * c,
                          delta_tower1_y * a + delta_tower2_y * b + delta_tower3_y * c,
                          actuator_mm[0] * a + actuator_mm[1] * b + actuator_mm[2] * c );

    float r_sq = 0.5F * q * magsq_s12 * magsq_s23 * magsq_s13;
    float dist = sqrtf(inv_nmag_sq * (arm_length_squared - r_sq));

    Vector3 cartesian = circumcenter.sub(normal.mul(dist));

    cartesian_mm[0] = ROUND(cartesian[0], 4);
    cartesian_mm[1] = ROUND(cartesian[1], 4);
    cartesian_mm[2] = ROUND(cartesian[2], 4);
}

bool LinearDeltaSolution::set_optional(const arm_options_t& options)
{

    for(auto &i : options) {
        switch(i.first) {
            case 'L': arm_length = i.second; break;
            case 'R': arm_radius = i.second; break;
            case 'A': tower1_offset = i.second; break;
            case 'B': tower2_offset = i.second; break;
            case 'C': tower3_offset = i.second; break;
            case 'D': tower1_angle = i.second; break;
            case 'E': tower2_angle = i.second; break;
            case 'F': tower3_angle = i.second; break; // WARNING this will be deprecated
            case 'H': tower3_angle = i.second; break;
        }
    }
    init();
    return true;
}

bool LinearDeltaSolution::get_optional(arm_options_t& options, bool force_all) const
{
    options['L'] = this->arm_length;
    options['R'] = this->arm_radius;

    // don't report these if none of them are set
    if(force_all || (this->tower1_offset != 0.0F || this->tower2_offset != 0.0F || this->tower3_offset != 0.0F ||
                     this->tower1_angle != 0.0F  || this->tower2_angle != 0.0F  || this->tower3_angle != 0.0F) ) {

        options['A'] = this->tower1_offset;
        options['B'] = this->tower2_offset;
        options['C'] = this->tower3_offset;
        options['D'] = this->tower1_angle;
        options['E'] = this->tower2_angle;
        options['H'] = this->tower3_angle;
    }

    return true;
};

