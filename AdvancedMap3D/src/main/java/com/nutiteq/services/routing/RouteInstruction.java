package com.nutiteq.services.routing;

import com.nutiteq.components.MapPos;

/**
 * Data object for route instruction
 */
public class RouteInstruction {
  private final int instructionType;
  private final DurationTime duration;
  private final int instructionNumber;
  private final String instruction;
  private final Distance distance;
  private final MapPos point;

  /**
   * 
   * @param instructionNumber
   *          number of this instruction
   * @param instructionType
   *          type of this instruction
   * @param duration
   *          duration to this point
   * @param instruction
   *          instruction for this point
   * @param distance
   *          distance to this point
   * @param point
   *          point location in WGS84
   */
  public RouteInstruction(final int instructionNumber, final int instructionType,
      final DurationTime duration, final String instruction, final Distance distance,
      final MapPos point) {
    this.instructionNumber = instructionNumber;
    this.instructionType = instructionType;
    this.duration = duration;
    this.instruction = instruction;
    this.distance = distance;
    this.point = point;
  }

  public int getInstructionNumber() {
    return instructionNumber;
  }

  public MapPos getPoint() {
    return point;
  }

  public String getInstruction() {
    return instruction;
  }

  public int getInstructionType() {
    return instructionType;
  }

  public Distance getDistance() {
    return distance;
  }

  public DurationTime getDuration() {
    return duration;
  }
}
