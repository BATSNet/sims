// This is a basic Flutter widget test for SIMS app.

import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';

import 'package:sims_app/main.dart';

void main() {
  testWidgets('SIMS app smoke test', (WidgetTester tester) async {
    // Build our app and trigger a frame.
    await tester.pumpWidget(const SimsApp());

    // Wait for async operations
    await tester.pumpAndSettle();

    // Verify that SIMS title or phone number screen appears
    expect(find.text('SIMS'), findsWidgets);
  });
}
