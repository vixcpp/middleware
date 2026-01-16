/**
 *
 *  @file rbac.hpp
 *  @author Gaspard Kirira
 *
 *  Copyright 2025, Gaspard Kirira.  All rights reserved.
 *  https://github.com/vixcpp/vix
 *  Use of this source code is governed by a MIT license
 *  that can be found in the License file.
 *
 *  Vix.cpp
 */
#ifndef VIX_RBAC_HPP
#define VIX_RBAC_HPP

#include <algorithm>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_set>
#include <utility>
#include <vector>

#include <nlohmann/json.hpp>

#include <vix/middleware/auth/jwt.hpp>

namespace vix::middleware::auth
{
  struct Authz final
  {
    std::string subject;
    std::unordered_set<std::string> roles;
    std::unordered_set<std::string> perms;

    bool has_role(std::string_view r) const
    {
      return roles.find(std::string(r)) != roles.end();
    }

    bool has_perm(std::string_view p) const
    {
      return perms.find(std::string(p)) != perms.end();
    }
  };

  struct PermissionResolver
  {
    virtual ~PermissionResolver() = default;
    virtual void resolve(
        std::string_view subject,
        std::unordered_set<std::string> &roles_inout,
        std::unordered_set<std::string> &perms_inout) = 0;
  };

  struct RbacOptions
  {
    std::string roles_key{"roles"};
    std::string perms_key{"perms"};
    bool require_auth{true};
    bool use_resolver{true};
  };

  inline void insert_if_string(std::unordered_set<std::string> &out, const nlohmann::json &v)
  {
    if (v.is_string())
      out.insert(v.get<std::string>());
  }

  inline void insert_strings_from_json(std::unordered_set<std::string> &out, const nlohmann::json &v)
  {
    if (v.is_string())
    {
      out.insert(v.get<std::string>());
      return;
    }
    if (v.is_array())
    {
      for (auto const &x : v)
        if (x.is_string())
          out.insert(x.get<std::string>());
    }
  }

  inline std::unordered_set<std::string> extract_set(
      const nlohmann::json &payload,
      std::string_view key_primary,
      std::string_view key_fallback = {})
  {
    std::unordered_set<std::string> out;

    if (!key_primary.empty() && payload.contains(std::string(key_primary)))
      insert_strings_from_json(out, payload.at(std::string(key_primary)));

    if (out.empty() && !key_fallback.empty() && payload.contains(std::string(key_fallback)))
      insert_strings_from_json(out, payload.at(std::string(key_fallback)));

    return out;
  }

  inline Authz build_authz_from_jwt(const JwtClaims &claims, const RbacOptions &opt)
  {
    Authz a;
    a.subject = claims.subject;

    a.roles = extract_set(claims.payload, opt.roles_key, "role");
    a.perms = extract_set(claims.payload, opt.perms_key, "permissions");

    if (claims.payload.contains("scope") && claims.payload["scope"].is_string())
    {
      const std::string scope = claims.payload["scope"].get<std::string>();
      std::string token;
      token.reserve(scope.size());

      for (char c : scope)
      {
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r')
        {
          if (!token.empty())
          {
            a.perms.insert(token);
            token.clear();
          }
        }
        else
        {
          token.push_back(c);
        }
      }
      if (!token.empty())
        a.perms.insert(token);
    }

    return a;
  }

  inline void maybe_enrich_with_resolver(Context &ctx, Authz &a, const RbacOptions &opt)
  {
    if (!opt.use_resolver)
      return;

    auto resolver = ctx.services().get<PermissionResolver>();
    if (!resolver)
      return;

    resolver->resolve(a.subject, a.roles, a.perms);
  }

  inline void send_forbidden(
      Context &ctx,
      std::string code,
      std::string message,
      std::unordered_map<std::string, std::string> details = {})
  {
    Error e;
    e.status = 403;
    e.code = std::move(code);
    e.message = std::move(message);
    e.details = std::move(details);
    ctx.send_error(normalize(std::move(e)));
  }

  inline void send_unauthorized(Context &ctx, std::string code, std::string message)
  {
    Error e;
    e.status = 401;
    e.code = std::move(code);
    e.message = std::move(message);
    ctx.res().header("WWW-Authenticate", "Bearer");
    ctx.send_error(normalize(std::move(e)));
  }

  inline MiddlewareFn rbac_context(RbacOptions opt = {})
  {
    return [opt = std::move(opt)](Context &ctx, Next next) mutable
    {
      auto *jwt = ctx.try_state<JwtClaims>();

      if (!jwt)
      {
        if (opt.require_auth)
        {
          send_unauthorized(ctx, "missing_auth", "Authentication required");
          return;
        }

        next();
        return;
      }

      Authz a = build_authz_from_jwt(*jwt, opt);
      maybe_enrich_with_resolver(ctx, a, opt);

      ctx.emplace_state<Authz>(std::move(a));

      next();
    };
  }

  inline MiddlewareFn require_role(std::string role)
  {
    return [role = std::move(role)](Context &ctx, Next next)
    {
      auto *a = ctx.try_state<Authz>();
      if (!a)
      {
        send_unauthorized(ctx, "missing_authz", "Authorization context missing (rbac_context not installed)");
        return;
      }

      if (!a->has_role(role))
      {
        send_forbidden(ctx, "forbidden", "Missing required role", {{"role", role}});
        return;
      }

      next();
    };
  }

  inline MiddlewareFn require_any_role(std::vector<std::string> roles)
  {
    return [roles = std::move(roles)](Context &ctx, Next next)
    {
      auto *a = ctx.try_state<Authz>();
      if (!a)
      {
        send_unauthorized(ctx, "missing_authz", "Authorization context missing (rbac_context not installed)");
        return;
      }

      for (auto const &r : roles)
        if (a->has_role(r))
        {
          next();
          return;
        }

      send_forbidden(ctx, "forbidden", "Missing any of required roles");
    };
  }

  inline MiddlewareFn require_perm(std::string perm)
  {
    return [perm = std::move(perm)](Context &ctx, Next next)
    {
      auto *a = ctx.try_state<Authz>();
      if (!a)
      {
        send_unauthorized(ctx, "missing_authz", "Authorization context missing (rbac_context not installed)");
        return;
      }

      if (!a->has_perm(perm))
      {
        send_forbidden(ctx, "forbidden", "Missing required permission", {{"perm", perm}});
        return;
      }

      next();
    };
  }

  inline MiddlewareFn require_any_perm(std::vector<std::string> perms)
  {
    return [perms = std::move(perms)](Context &ctx, Next next)
    {
      auto *a = ctx.try_state<Authz>();
      if (!a)
      {
        send_unauthorized(ctx, "missing_authz", "Authorization context missing (rbac_context not installed)");
        return;
      }

      for (auto const &p : perms)
        if (a->has_perm(p))
        {
          next();
          return;
        }

      send_forbidden(ctx, "forbidden", "Missing any of required permissions");
    };
  }

  inline MiddlewareFn require_all_perms(std::vector<std::string> perms)
  {
    return [perms = std::move(perms)](Context &ctx, Next next)
    {
      auto *a = ctx.try_state<Authz>();
      if (!a)
      {
        send_unauthorized(ctx, "missing_authz", "Authorization context missing (rbac_context not installed)");
        return;
      }

      for (auto const &p : perms)
      {
        if (!a->has_perm(p))
        {
          send_forbidden(ctx, "forbidden", "Missing required permission", {{"perm", p}});
          return;
        }
      }

      next();
    };
  }

} // namespace vix::middleware::auth

#endif
