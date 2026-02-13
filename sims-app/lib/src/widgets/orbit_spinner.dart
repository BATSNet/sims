import 'package:flutter/material.dart';
import 'dart:math' as math;

class OrbitSpinner extends StatefulWidget {
  final double size;
  final Color orbitOneColor;
  final Color orbitTwoColor;
  final Color orbitThreeColor;
  final Duration duration;
  final double strokeWidth;

  const OrbitSpinner({
    super.key,
    this.size = 34.0,
    this.orbitOneColor = const Color(0xFF2DD4BF),  // Accent cyan
    this.orbitTwoColor = const Color(0xFF4A7C59),  // Tactical green
    this.orbitThreeColor = const Color(0xFF334155),  // Slate 700
    this.duration = const Duration(milliseconds: 1200),
    this.strokeWidth = 3.0,
  });

  @override
  _OrbitSpinnerState createState() => _OrbitSpinnerState();
}

class _OrbitSpinnerState extends State<OrbitSpinner> with SingleTickerProviderStateMixin {
  late AnimationController _controller;

  @override
  void initState() {
    super.initState();
    _controller = AnimationController(
      vsync: this,
      duration: widget.duration,
    )..repeat();
  }

  @override
  void dispose() {
    _controller.dispose();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    return SizedBox(
      width: widget.size,
      height: widget.size,
      child: Stack(
        children: [
          _buildOrbit(widget.orbitOneColor, 35, -45, OrbitPosition.bottom, widget.strokeWidth),
          _buildOrbit(widget.orbitTwoColor, 55, 10, OrbitPosition.right, widget.strokeWidth),
          _buildOrbit(widget.orbitThreeColor, 35, 55, OrbitPosition.top, widget.strokeWidth),
        ],
      ),
    );
  }

  Widget _buildOrbit(Color color, double rotateX, double rotateY, OrbitPosition position, double strokeWidth) {
    return AnimatedBuilder(
      animation: _controller,
      builder: (context, child) {
        return Transform(
          transform: Matrix4.identity()
            ..rotateX(rotateX * math.pi / 180)
            ..rotateY(rotateY * math.pi / 180)
            ..rotateZ(_controller.value * 2 * math.pi),
          alignment: Alignment.center,
          child: CustomPaint(
            size: Size(widget.size, widget.size),
            painter: OrbitPainter(
              color: color,
              position: position,
              strokeWidth: strokeWidth,
            ),
          ),
        );
      },
    );
  }
}

enum OrbitPosition { top, right, bottom }

class OrbitPainter extends CustomPainter {
  final Color color;
  final OrbitPosition position;
  final double strokeWidth;

  OrbitPainter({required this.color, required this.position, required this.strokeWidth});

  @override
  void paint(Canvas canvas, Size size) {
    final rect = Rect.fromLTWH(0, 0, size.width, size.height);
    final path = Path();

    switch (position) {
      case OrbitPosition.top:
        path.addArc(rect, -math.pi / 2, math.pi);
        break;
      case OrbitPosition.right:
        path.addArc(rect, 0, math.pi);
        break;
      case OrbitPosition.bottom:
        path.addArc(rect, math.pi / 2, math.pi);
        break;
    }

    final paint = Paint()
      ..color = color
      ..style = PaintingStyle.stroke
      ..strokeWidth = strokeWidth
      ..strokeCap = StrokeCap.butt;

    final Shader gradientShader = LinearGradient(
      colors: [
        color.withOpacity(0.3),
        color.withOpacity(1),
        color.withOpacity(0.6),
      ],
      stops: const [0.4, 0.8, 1],
      begin: Alignment.topCenter,
      end: Alignment.bottomCenter,
    ).createShader(rect);

    paint.shader = gradientShader;

    canvas.drawPath(path, paint);
  }

  @override
  bool shouldRepaint(OrbitPainter oldDelegate) {
    return color != oldDelegate.color ||
        position != oldDelegate.position ||
        strokeWidth != oldDelegate.strokeWidth;
  }
}
