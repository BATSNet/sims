import 'package:flutter/material.dart';

abstract final class SimsColors {
  static const Color transparent = Color(0x00000000);

  // Brand Colors - Navy Blue and Accent Blue
  static const Color navyBlue = Color(0xFF0D2637);  // Primary navy blue background (from logo)
  static const Color navyBlueDark = Color(0xFF050B14);  // Darker navy for depth
  static const Color navyBlueLight = Color(0xFF1A2E42);  // Lighter navy for cards
  static const Color accentBlue = Color(0xFF3B82F6);  // Accent blue for CTAs (friendly)
  static const Color accentTeal = Color(0xFF14B8A6);  // Legacy teal color
  static const Color accentRed = Color(0xFFDC2626);  // Accent red kept for critical alerts only

  // Grayscale
  static const Color white = Color(0xFFFFFFFF);
  static const Color offWhite = Color(0xFFF5F5F5);
  static const Color grayNeutral = Color(0xFFE5E7EB);
  static const Color lightGray = Color(0xFFE5E7EB);
  static const Color gray = Color(0xFF6B7280);
  static const Color gray600 = Color(0xFF6B7280);
  static const Color darkGray = Color(0xFF374151);
  static const Color dark = Color(0xFF111827);
  static const Color almostBlack = Color(0xFF111827);

  // Legacy colors for compatibility
  static const Color blue = navyBlue;
  static const Color blue600 = Color(0xFF2563eb);
  static const Color teal = Color(0xFF51b5bf);
  static const Color green600 = Color(0xFF10B981);

  // Priority colors (for incidents)
  static const Color criticalRed = Color(0xFFDC2626);
  static const Color highOrange = Color(0xFFEA580C);
  static const Color mediumBlue = accentBlue;  // Use same blue as accent
  static const Color lowGreen = Color(0xFF10B981);

  // Status colors
  static const Color statusOpen = Color(0xFFEA580C);
  static const Color statusInProgress = accentBlue;  // Use same blue as accent
  static const Color statusResolved = Color(0xFF10B981);
  static const Color statusClosed = Color(0xFF6B7280);

  // Gradients for incident priorities (no rounded corners will be applied in UI)
  static const LinearGradient criticalGradient = LinearGradient(
    begin: Alignment.centerLeft,
    end: Alignment.centerRight,
    colors: [
      Color(0xFFEF4444),
      Color(0xFFDC2626),
    ],
  );

  static const LinearGradient highGradient = LinearGradient(
    begin: Alignment.centerLeft,
    end: Alignment.centerRight,
    colors: [
      Color(0xFFF97316),
      Color(0xFFEA580C),
    ],
  );

  static const LinearGradient mediumGradient = LinearGradient(
    begin: Alignment.centerLeft,
    end: Alignment.centerRight,
    colors: [
      Color(0xFF3B82F6),
      Color(0xFF2563EB),
    ],
  );

  static const LinearGradient lowGradient = LinearGradient(
    begin: Alignment.centerLeft,
    end: Alignment.centerRight,
    colors: [
      Color(0xFF10B981),
      Color(0xFF059669),
    ],
  );

  // Navy blue gradient for general use
  static const LinearGradient navyGradient = LinearGradient(
    begin: Alignment.topCenter,
    end: Alignment.bottomCenter,
    colors: [
      SimsColors.navyBlueDark,
      SimsColors.navyBlue,
    ],
  );

  // Alias for compatibility
  static const LinearGradient blueGradient = navyGradient;
}
