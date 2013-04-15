package com.nutiteq.services.routing;

import java.util.concurrent.TimeUnit;

/**
 * Generic duration object containing days, hours, minutes and seconds.
 */
public class DurationTime {
  private final int days;
  private final int hours;
  private final int minutes;
  private final int seconds;

  public DurationTime(final int days, final int hours, final int minutes, final int seconds) {
    this.days = days;
    this.hours = hours;
    this.minutes = minutes;
    this.seconds = seconds;
  }

  public DurationTime() {
    this(0, 0, 0, 0);
  }

  public DurationTime(long seconds) {
      this.seconds = (int) (seconds ) % 60 ;
      this.minutes = (int) ((seconds / 60) % 60);
      this.hours   = (int) ((seconds / (60*60)) % 24);
      this.days    = (int) ((seconds / (60*60*24)));
    
  }

public int getDays() {
    return days;
  }

  public int getHours() {
    return hours;
  }

  public int getMinutes() {
    return minutes;
  }

  public int getSeconds() {
    return seconds;
  }
  
  @Override
  public String toString(){
    return ((days > 0) ? days + " d " : "") +
            ((hours > 0) ? hours + " h " : "") +
            ((minutes > 0) ? minutes + " m " : "") +
            seconds + " s";
  }
}
