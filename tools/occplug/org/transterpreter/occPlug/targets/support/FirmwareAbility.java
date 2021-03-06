package org.transterpreter.occPlug.targets.support;

/*
 * FirmwareAbility.java
 * part of the occPlug plugin for the jEdit text editor
 * Copyright (C) 2009 Christian L. Jacobsen
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

import javax.swing.JPanel;

public interface FirmwareAbility {

	public FirmwareTarget[] getFirmwareTargets();

	public void uploadFirmware(FirmwareTarget target, Runnable finished);

	public JPanel getFirmwareOptions(FirmwareTarget target);

	public void setEnabledForFirmwareOptions(boolean enabled);
}
