import 'package:equatable/equatable.dart';

class User extends Equatable {
  final String id;
  final String phoneNumber;
  final String? name;

  const User({
    required this.id,
    required this.phoneNumber,
    this.name,
  });

  factory User.fromPhoneNumber(String phoneNumber) {
    return User(
      id: phoneNumber,
      phoneNumber: phoneNumber,
    );
  }

  User copyWith({
    String? id,
    String? phoneNumber,
    String? name,
  }) {
    return User(
      id: id ?? this.id,
      phoneNumber: phoneNumber ?? this.phoneNumber,
      name: name ?? this.name,
    );
  }

  Map<String, dynamic> toJson() {
    return {
      'id': id,
      'phoneNumber': phoneNumber,
      'name': name,
    };
  }

  factory User.fromJson(Map<String, dynamic> json) {
    return User(
      id: json['id'] as String,
      phoneNumber: json['phoneNumber'] as String,
      name: json['name'] as String?,
    );
  }

  @override
  List<Object?> get props => [id, phoneNumber, name];
}
