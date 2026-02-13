import 'package:flutter/material.dart';

abstract final class SimsColors {
  static const Color transparent = Color(0x00000000);

  // Modern Tactical Base Colors - Deep slate/charcoal with subtle olive undertones
  static const Color background = Color(0xFF0A0E12);  // Deep tactical slate
  static const Color backgroundLight = Color(0xFF121820);  // Cards, elevated surfaces
  static const Color backgroundElevated = Color(0xFF1A2028);  // Modals, overlays

  // Tactical Accents - Muted, sophisticated colors
  static const Color accentTactical = Color(0xFF4A7C59);  // Muted olive-green for primary CTAs
  static const Color accentCyan = Color(0xFF2DD4BF);  // Status indicators, live updates
  static const Color accentAmber = Color(0xFFF59E0B);  // Warnings, medium priority

  // Slate Neutrals for depth and hierarchy
  static const Color slate900 = Color(0xFF0F172A);
  static const Color slate800 = Color(0xFF1E293B);
  static const Color slate700 = Color(0xFF334155);
  static const Color slate600 = Color(0xFF475569);
  static const Color slate500 = Color(0xFF64748B);

  // Legacy Brand Colors - Maintained for backward compatibility
  static const Color navyBlue = background;  // Map to new background
  static const Color navyBlueDark = Color(0xFF050B14);
  static const Color navyBlueLight = backgroundLight;  // Map to new backgroundLight
  static const Color accentBlue = accentCyan;  // Map to modern cyan
  static const Color accentTeal = Color(0xFF14B8A6);
  static const Color accentRed = Color(0xFFDC2626);

  // Grayscale
  static const Color white = Color(0xFFFFFFFF);
  static const Color offWhite = Color(0xFFF5F5F5);
  static const Color grayNeutral = Color(0xFFE5E7EB);
  static const Color lightGray = Color(0xFFE5E7EB);
  static const Color gray = slate500;
  static const Color gray600 = slate600;
  static const Color darkGray = slate700;
  static const Color dark = slate900;
  static const Color almostBlack = background;

  // Legacy colors for compatibility
  static const Color blue = background;
  static const Color blue600 = Color(0xFF2563eb);
  static const Color teal = Color(0xFF51b5bf);
  static const Color green600 = accentTactical;

  // Modern Priority Colors - Muted tactical palette
  static const Color criticalRed = Color(0xFFB91C1C);  // Deep red
  static const Color highOrange = Color(0xFFD97706);  // Burnt orange
  static const Color mediumBlue = accentCyan;  // Tactical cyan
  static const Color lowGreen = accentTactical;  // Muted tactical green

  // Status colors
  static const Color statusOpen = highOrange;
  static const Color statusInProgress = accentCyan;
  static const Color statusResolved = accentTactical;
  static const Color statusClosed = slate600;

  // Modern Priority Gradients - Muted tactical tones with subtle sophistication
  static const LinearGradient criticalGradient = LinearGradient(
    begin: Alignment.centerLeft,
    end: Alignment.centerRight,
    colors: [
      Color(0xFFDC2626),  // Deep red
      Color(0xFFC2410C),  // Burnt orange
    ],
  );

  static const LinearGradient highGradient = LinearGradient(
    begin: Alignment.centerLeft,
    end: Alignment.centerRight,
    colors: [
      Color(0xFFC2410C),  // Burnt orange
      Color(0xFFF59E0B),  // Amber
    ],
  );

  static const LinearGradient mediumGradient = LinearGradient(
    begin: Alignment.centerLeft,
    end: Alignment.centerRight,
    colors: [
      accentTactical,  // Tactical green
      accentCyan,  // Cyan
    ],
  );

  static const LinearGradient lowGradient = LinearGradient(
    begin: Alignment.centerLeft,
    end: Alignment.centerRight,
    colors: [
      slate700,  // Muted slate
      accentTactical,  // Tactical green
    ],
  );

  // Background gradient for general use
  static const LinearGradient navyGradient = LinearGradient(
    begin: Alignment.topCenter,
    end: Alignment.bottomCenter,
    colors: [
      background,
      backgroundLight,
    ],
  );

  // Alias for compatibility
  static const LinearGradient blueGradient = navyGradient;
}
