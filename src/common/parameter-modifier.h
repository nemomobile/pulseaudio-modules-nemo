/*
 * Copyright (C) 2011 Nokia Corporation.
 *
 * Contact: Maemo MMF Audio <mmf-audio@projects.maemo.org>
 *          or Jyri Sarha <jyri.sarha@nokia.com>
 *
 * These PulseAudio Modules are free software; you can redistribute
 * it and/or modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation
 * version 2.1 of the License.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301
 * USA.
 */
#ifndef _parameter_modifier_h_
#define _parameter_modifier_h_

/**
 * This header defines the interface visible to parameter modifier implementors.
 *
 *  A parameter modifier provides custom parameters for a specific algorithm in
 *  a specific mode. Thus, it allows default parameters to be overridden during
 *  runtime by either copying them and modifying parts of them or replacing
 *  them entirely from scratch.
 */

/**
 * The structure that defines the parameter modifier interface provided and
 * owned by the modifier implementor.
 *
 * The implementor must register this struct, preserve it during use,
 * unregister it when done and subsequently deallocate it if needed.
 */
typedef struct {

    /* The name of the mode in which algorithm parameters are affected. */
    const char *mode_name;

    /* The name of the algorithm whose parameters this modifier affects. */
    const char *algorithm_name;

    /**
     * Get parameters, possibly based on base parameters read from the file
     * system.
     *
     * This function is called:
     *   * Exactly once every time the mode changes to mode_name and the
     *     algorithm matching algorithm_name is present.
     *   * When the modifier is registered (see
     *     meego_parameter_register_modifier) and the current mode is mode_name
     *     and the algorithm matching algorithm_name is present.
     *   * When a previously nonexistent algorithm that matches algorithm_name
     *     starts listening to parameters and the current mode is mode_name.
     *
     * return value
     *     TRUE when successful, FALSE when unsuccessful. If TRUE is
     *     returned, the algorithm is updated using the returned parameters.
     *     Otherwise file system defaults are used. If no file system defaults
     *     exist and FALSE is returned, the algorithm is disabled.
     *
     * base_parameters
     *     Parameters that the implementor can copy and use as a basis for
     *     the modified parameters. If no file system defaults exist, this value
     *     can be NULL in which case the implementor must either cope without base
     *     parameters or return FALSE to indicate an error (thus disabling the
     *     algorithm).
     *
     * len_base_parameters
     *     The length of base_parameters
     *
     * parameters
     *     The modified parameters provided by this function. With TRUE return
     *     value, a non-NULL value must be written to *parameters.
     *
     * len_parameters
     *     The length of the modified parameters. With TRUE return value, a
     *     value > 0 must be written to *len_parameters.
     *
     * userdata
     *     The value of the userdata member of this struct.
     */
    pa_bool_t (*get_parameters)(const void *base_parameters, unsigned len_base_parameters, void **parameters, unsigned *len_parameters, void *userdata);

    /* Data for the implementor's own use. Passed as the final parameter to get_parameters. */
    void *userdata;

} meego_parameter_modifier;

/**
 * Register a modifier for a specific algorithm in a specific mode. Registered
 * modifiers must always be unregistered after use. Multiple modifiers cannot
 * be registered for the same (mode, algorithm) pair.
 * Returns either 0 or a negative error code.
 */
int meego_parameter_register_modifier(meego_parameter_modifier *modifier);

/**
 * Unregister a modifier. 'modifier' must point to the same struct that was
 * previously registered.
 * Returns either 0 or a negative error code.
 */
int meego_parameter_unregister_modifier(meego_parameter_modifier *modifier);

#endif
